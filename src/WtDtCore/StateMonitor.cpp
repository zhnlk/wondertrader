#include "StateMonitor.h"
#include "DataManager.h"

#include "../Share/StdUtils.hpp"
#include "../Share/TimeUtils.hpp"
#include "../Share/StrUtil.hpp"
#include "../Share/WTSContractInfo.hpp"
#include "../Share/WTSSessionInfo.hpp"

#include "../WTSTools/WTSBaseDataMgr.h"
#include "../WTSTools/WTSLogger.h"

#include <rapidjson/document.h>
namespace rj = rapidjson;


StateMonitor::StateMonitor()
	: _stopped(false)
	, _bd_mgr(NULL)
	, _dt_mgr(NULL)
{
}


StateMonitor::~StateMonitor()
{
}

bool StateMonitor::initialize(const char* filename, WTSBaseDataMgr* bdMgr, DataManager* dtMgr)
{
	_bd_mgr = bdMgr;
	_dt_mgr = dtMgr;

	if (!StdFile::exists(filename))
	{
		WTSLogger::error("״̬�����ļ� %s ������", filename);
		return false;
	}

	std::string content;
	StdFile::read_file_content(filename, content);
	rj::Document root;
	if (root.Parse(content.c_str()).HasParseError())
	{
		WTSLogger::error("״̬�����ļ�����ʧ��");
		return false;
	}

	for (auto& m : root.GetObject())
	{
		const char* sid = m.name.GetString();
		const rj::Value& jItem = m.value;

		WTSSessionInfo* ssInfo = _bd_mgr->getSession(sid);
		if (ssInfo == NULL)
		{
			WTSLogger::error("����ʱ��ģ��[%s]�����ڣ���������״̬��������", sid);
			continue;
		}

		StatePtr sInfo(new StateInfo);
		sInfo->_init_time = jItem["inittime"].GetUint();	//��ʼ��ʱ�䣬��ʼ���Ժ����ݲſ�ʼ����
		sInfo->_close_time = jItem["closetime"].GetUint();	//����ʱ�䣬���̺����ݲ��ٽ�����
		sInfo->_proc_time = jItem["proctime"].GetUint();	//�̺���ʱ�䣬��Ҫ��ʵʱ����ת����ʷȥ

		strcpy(sInfo->_session, sid);

		auto secInfo = ssInfo->getAuctionSection();//��������ƫ�ƹ���ʱ�䣬Ҫע����!!!
		if (secInfo.first != 0 || secInfo.second != 0)
		{
			uint32_t stime = secInfo.first;
			uint32_t etime = secInfo.second;

			stime = stime / 100 * 60 + stime % 100;//�Ƚ�ʱ��ת�ɷ�����
			etime = etime / 100 * 60 + etime % 100;

			stime = stime / 60 * 100 + stime % 60;//�ٽ�������ת��ʱ��
			etime = etime / 60 * 100 + etime % 60;//�Ȳ����ǰ�ҹ12��������Ŀǰ����������û��
			sInfo->_sections.push_back(StateInfo::Section({ stime, etime }));
		}

		auto sections = ssInfo->getTradingSections();//��������ƫ�ƹ���ʱ�䣬Ҫע����!!!
		for (auto it = sections.begin(); it != sections.end(); it++)
		{
			auto secInfo = *it;
			uint32_t stime = secInfo.first;
			uint32_t etime = secInfo.second;

			stime = stime / 100 * 60 + stime % 100;//�Ƚ�ʱ��ת�ɷ�����
			etime = etime / 100 * 60 + etime % 100;

			stime--;//��ʼ������-1
			etime++;//����������+1

			stime = stime / 60 * 100 + stime % 60;//�ٽ�������ת��ʱ��
			etime = etime / 60 * 100 + etime % 60;//�Ȳ����ǰ�ҹ12��������Ŀǰ����������û��
			sInfo->_sections.push_back(StateInfo::Section({ stime, etime }));
		}

		_map[sInfo->_session] = sInfo;

		CodeSet* pCommSet =  _bd_mgr->getSessionComms(sInfo->_session);
		if (pCommSet)
		{
			uint32_t curDate = TimeUtils::getCurDate();
			uint32_t curMin = TimeUtils::getCurMin() / 100;
			uint32_t offDate = ssInfo->getOffsetDate(curDate, curMin);
			uint32_t offMin = ssInfo->offsetTime(curMin);

			//�Ȼ�ȡ��׼�Ľ�����

			for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
			{
				const char* pid = (*it).c_str();

				 _bd_mgr->setTradingDate(pid,  _bd_mgr->getTradingDate(pid, offDate, offMin, false), false);
				uint32_t prevDate = TimeUtils::getNextDate(curDate, -1);
				if ((ssInfo->getOffsetMins() > 0 &&
					(! _bd_mgr->isTradingDate(pid, curDate) &&
					!(ssInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||
					(ssInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))
					)
				{
					WTSLogger::info("Ʒ��%s���ڽڼ���", pid);
				}
			}
		}
	}

	return true;
}

void StateMonitor::run()
{
	if(_thrd == NULL)
	{
		_thrd.reset(new BoostThread([this](){

			while (!_stopped)
			{
				static uint64_t lastTime = 0;

				while(true)
				{
					uint64_t now = TimeUtils::getLocalTimeNow();
					if(now - lastTime >= 1000)
						break;

					if(_stopped)
						break;

					boost::this_thread::sleep(boost::posix_time::milliseconds(2));
				}

				if(_stopped)
					break;

				uint32_t curDate = TimeUtils::getCurDate();
				uint32_t curMin = TimeUtils::getCurMin() / 100;

				auto it = _map.begin();
				for (; it != _map.end(); it++)
				{
					StatePtr& sInfo = it->second;
					WTSSessionInfo* mInfo =  _bd_mgr->getSession(sInfo->_session);

					uint32_t offDate = mInfo->getOffsetDate(curDate, curMin);
					uint32_t prevDate = TimeUtils::getNextDate(curDate, -1);

					switch(sInfo->_state)
					{
					case SS_ORIGINAL:
						{
							uint32_t offTime = mInfo->offsetTime(curMin);
							uint32_t offInitTime = mInfo->offsetTime(sInfo->_init_time);
							uint32_t offCloseTime = mInfo->offsetTime(sInfo->_close_time);
							uint32_t aucStartTime = mInfo->getAuctionStartTime(true);

							bool isAllHoliday = true;
							std::stringstream ss_a, ss_b;
							CodeSet* pCommSet =  _bd_mgr->getSessionComms(sInfo->_session);
							if (pCommSet)
							{
								for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
								{
									const char* pid = (*it).c_str();
									/*
									 *	���ʱ������ƫ��
									 *	�����ǰ���ڲ��ǽ����գ��Ҳ�����ҹ�̺��ҹ������ʱ���������ǽ����գ�
									 *	����ʱ������ƫ�ƵĻ����Ϳ�ƫ�������Ƿ��ǽڼ���
									 */
									if ((mInfo->getOffsetMins() > 0 &&
										(! _bd_mgr->isTradingDate(pid, curDate) &&	//��ǰ��־���ǽ�����
										!(mInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||	//��ǰ���ڽ���ʱ�䣬�������ǽ�����
										(mInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))
										)
									{
										ss_a << pid << ",";
										WTSLogger::info("Ʒ��%s���ڽڼ���", pid);
									}
									else
									{
										ss_b << pid << ",";
										isAllHoliday = false;
									}
								}

								//WTSLogger::info("����ʱ��ģ�� %s[%s]����Ʒ�֣�%s���ݼ�Ʒ�֣�%s", mInfo->name(), sInfo->_session, ss_b.str().c_str(), ss_a.str().c_str());
							}
							else
							{
								WTSLogger::info("����ʱ��ģ�� %s[%s]û�й���Ʒ�֣������ݼ�״̬", mInfo->name(), sInfo->_session);
								sInfo->_state = SS_Holiday;
							}

							if(isAllHoliday)
							{
								WTSLogger::info("����ʱ��ģ�� %s[%s]ȫ��Ʒ�ִ��ڽڼ��գ������ݼ�״̬", mInfo->name(), sInfo->_session);
								sInfo->_state = SS_Holiday;
							}
							else if (offTime >= offCloseTime)
							{
								sInfo->_state = SS_CLOSED;
								WTSLogger::info("����ʱ��ģ�� %s[%s]ֹͣ��������", mInfo->name(), sInfo->_session);
							}
							else if (aucStartTime != -1 && offTime >= aucStartTime)
							{
								if (sInfo->isInSections(offTime))
								{
									//if(sInfo->_schedule)
									//{
									//	_dt_mgr->preloadRtCaches();
									//}
									sInfo->_state = SS_RECEIVING;
									WTSLogger::info("����ʱ��ģ�� %s[%s]��ʼ��������", mInfo->name(), sInfo->_session);
								}
								else
								{
									//if (sInfo->_schedule)
									//{
									//	_dt_mgr->preloadRtCaches();
									//}
									sInfo->_state = SS_PAUSED;
									WTSLogger::info("����ʱ��ģ�� %s[%s]��ͣ��������", mInfo->name(), sInfo->_session);
								}
							}								
							else if (offTime >= offInitTime)
							{
								sInfo->_state = SS_INITIALIZED;
								WTSLogger::info("����ʱ��ģ�� %s[%s]��ʼ��", mInfo->name(), sInfo->_session);
							}

							
						}
						break;
					case SS_INITIALIZED:
						{
							uint32_t offTime = mInfo->offsetTime(curMin);
							uint32_t offAucSTime = mInfo->getAuctionStartTime(true);
							if (offAucSTime == -1 || offTime >= mInfo->getAuctionStartTime(true))
							{
								if (!sInfo->isInSections(offTime) && offTime < mInfo->getCloseTime(true))
								{
									//if (sInfo->_schedule)
									//{
									//	_dt_mgr->preloadRtCaches();
									//}
									sInfo->_state = SS_PAUSED;

									WTSLogger::info("����ʱ��ģ�� %s[%s]��ͣ��������", mInfo->name(), sInfo->_session);
								}
								else
								{
									//if (sInfo->_schedule)
									//{
									//	_dt_mgr->preloadRtCaches();
									//}
									sInfo->_state = SS_RECEIVING;
									WTSLogger::info("����ʱ��ģ�� %s[%s]��ʼ��������", mInfo->name(), sInfo->_session);
								}
								
							}
						}
						break;
					case SS_RECEIVING:
						{
							uint32_t offTime = mInfo->offsetTime(curMin);
							uint32_t offCloseTime = mInfo->offsetTime(sInfo->_close_time);
							if (offTime >= offCloseTime)
							{
								sInfo->_state = SS_CLOSED;

								WTSLogger::info("����ʱ��ģ�� %s[%s]ֹͣ��������", mInfo->name(), sInfo->_session);
							}
							else if (offTime >= mInfo->getAuctionStartTime(true))
							{
								if (offTime < mInfo->getCloseTime(true))
								{
									if (!sInfo->isInSections(offTime))
									{
										//if (sInfo->_schedule)
										//{
										//	_dt_mgr->preloadRtCaches();
										//}
										sInfo->_state = SS_PAUSED;

										WTSLogger::info("����ʱ��ģ�� %s[%s]��ͣ��������", mInfo->name(), sInfo->_session);
									}
								}
								else
								{
									//��������������Ժ��ʱ��
									//���ﲻ�ܸ�״̬����ΪҪ�ս����
								}
							}
						}
						break;
					case SS_PAUSED:
						{
							//��Ϣ״ֻ̬��ת��Ϊ����״̬
							//����Ҫ��ƫ�ƹ������ڣ���Ȼ���������������;��Ϣ���ͻ����
							uint32_t weekDay = TimeUtils::getWeekDay();

							bool isAllHoliday = true;
							CodeSet* pCommSet =  _bd_mgr->getSessionComms(sInfo->_session);
							if (pCommSet)
							{
								for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
								{
									const char* pid = (*it).c_str();
									if ((mInfo->getOffsetMins() > 0 &&
										(! _bd_mgr->isTradingDate(pid, curDate) &&
										!(mInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||
										(mInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))
										)
									{
										WTSLogger::info("Ʒ��%s���ڽڼ���", pid);
									}
									else
									{
										isAllHoliday = false;
									}
								}
							}
							
							if (!isAllHoliday)
							{
								uint32_t offTime = mInfo->offsetTime(curMin);
								if (sInfo->isInSections(offTime))
								{
									sInfo->_state = SS_RECEIVING;
									WTSLogger::info("����ʱ��ģ�� %s[%s]�ָ���������", mInfo->name(), sInfo->_session);
								}
							}
							else
							{
								WTSLogger::info("����ʱ��ģ�� %s[%s]ȫ��Ʒ�ִ��ڽڼ��գ������ݼ�״̬", mInfo->name(), sInfo->_session);
								sInfo->_state = SS_Holiday;
							}
						}
						break;
					case SS_CLOSED:
						{
							uint32_t offTime = mInfo->offsetTime(curMin);
							uint32_t offProcTime = mInfo->offsetTime(sInfo->_proc_time);
							if (offTime >= offProcTime)
							{
								if(!_dt_mgr->isSessionProceeded(sInfo->_session))
								{
									sInfo->_state = SS_PROCING;

									WTSLogger::info("����ʱ��ģ�� %s[%s]��ʼ����������ҵ����", mInfo->name(), sInfo->_session);
									_dt_mgr->transHisData(sInfo->_session);
								}
								else
								{
									sInfo->_state = SS_PROCED;
								}
							}
							else if (offTime >= mInfo->getAuctionStartTime(true) && offTime <= mInfo->getCloseTime(true))
							{
								if (!sInfo->isInSections(offTime))
								{
									sInfo->_state = SS_PAUSED;

									WTSLogger::info("����ʱ��ģ�� %s[%s] ��ͣ��������", mInfo->name(), sInfo->_session);
								}
							}
						}
						break;
					case SS_PROCING:
						sInfo->_state = SS_PROCED;
						break;
					case SS_PROCED:
					case SS_Holiday:
						{
							uint32_t offTime = mInfo->offsetTime(curMin);
							uint32_t offInitTime = mInfo->offsetTime(sInfo->_init_time);
							if (offTime >= 0 && offTime < offInitTime)
							{
								bool isAllHoliday = true;
								CodeSet* pCommSet =  _bd_mgr->getSessionComms(sInfo->_session);
								if (pCommSet)
								{
									for (auto it = pCommSet->begin(); it != pCommSet->end(); it++)
									{
										const char* pid = (*it).c_str();
										if ((mInfo->getOffsetMins() > 0 &&
											(! _bd_mgr->isTradingDate(pid, curDate) &&
											!(mInfo->isInTradingTime(curMin) &&  _bd_mgr->isTradingDate(pid, prevDate)))) ||
											(mInfo->getOffsetMins() <= 0 && ! _bd_mgr->isTradingDate(pid, offDate))
											)
										{
											//WTSLogger::info("Ʒ��%s���ڽڼ���", pid);
										}
										else
										{
											isAllHoliday = false;
										}
									}
								}

								if(!isAllHoliday)
								{
									sInfo->_state = SS_ORIGINAL;
									WTSLogger::info("����ʱ��ģ�� %s[%s]������", mInfo->name(), sInfo->_session);
								}
							}
						}
						break;
					}
					
				}

				lastTime = TimeUtils::getLocalTimeNow();

				if (isAllInState(SS_PROCING) && !isAllInState(SS_Holiday))
				{
					//��������
					_dt_mgr->transHisData("CMD_CLEAR_CACHE");
				}
			}
		}));
	}
}

void StateMonitor::stop()
{
	_stopped = true;

	if (_thrd)
		_thrd->join();
}

bool StateMonitor::isAllInState(SimpleState ss) const
{
	auto it = _map.begin();
	for (; it != _map.end(); it++)
	{
		const StatePtr& sInfo = it->second;
		if (sInfo->_state != SS_Holiday && sInfo->_state != ss)
			return false;
	}

	return true;
}


bool StateMonitor::isAnyInState(SimpleState ss) const
{
	auto it = _map.begin();
	for (; it != _map.end(); it++)
	{
		const StatePtr& sInfo = it->second;
		if (sInfo->_state == ss)
			return true;
	}

	return false;
}

bool StateMonitor::isInState(const char* sid, SimpleState ss) const
{
	auto it = _map.find(sid);
	if (it == _map.end())
		return false;

	const StatePtr& sInfo = it->second;
	return sInfo->_state == ss;
}