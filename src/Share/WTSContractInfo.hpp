#pragma once
#include "WTSObject.hpp"
#include "WTSTypes.h"
#include <string>
#include <unordered_set>

NS_OTP_BEGIN

class WTSCommodityInfo: public WTSObject
{
public:
	static WTSCommodityInfo* create(const char* pid, const char* name, const char* exchg, const char* session, const char* trdtpl, const char* currency = "CNY")
	{
		WTSCommodityInfo* ret = new WTSCommodityInfo;
		ret->m_strName = name;
		ret->m_strExchg = exchg;
		ret->m_strProduct = pid;
		ret->m_strCurrency = currency;
		ret->m_strSession = session;
		ret->m_strTrdTpl = trdtpl;

		return ret;
	}

	void	setVolScale(uint32_t volScale){ m_uVolScale = volScale; }
	void	setPriceTick(double pxTick){ m_fPriceTick = pxTick; }
	void	setCategory(ContractCategory cat){ m_ccCategory = cat; }
	void	setCoverMode(CoverMode cm){ m_coverMode = cm; }
	void	setPriceMode(PriceMode pm){ m_priceMode = pm; }
	void	setPrecision(uint32_t prec){ m_uPrecision = prec; }

	const char* getName()	const{ return m_strName.c_str(); }
	const char* getExchg()	const{ return m_strExchg.c_str(); }
	const char* getProduct()	const{ return m_strProduct.c_str(); }
	const char* getCurrency()	const{ return m_strCurrency.c_str(); }
	const char* getSession()	const{ return m_strSession.c_str(); }
	const char* getTradingTpl()	const{ return m_strTrdTpl.c_str(); }

	uint32_t	getVolScale()	const{ return m_uVolScale; }
	double		getPriceTick()	const{ return m_fPriceTick; }
	uint32_t	getPrecision()	const{ return m_uPrecision; }

	ContractCategory	getCategoty() const{ return m_ccCategory; }
	CoverMode			getCoverMode() const{ return m_coverMode; }
	PriceMode			getPriceMode() const{ return m_priceMode; }

	void		addCode(const char* code){ m_setCodes.insert(code); }
	const std::unordered_set<std::string>& getCodes() const{ return m_setCodes; }

private:
	std::string	m_strName;
	std::string	m_strExchg;
	std::string	m_strProduct;
	std::string	m_strCurrency;
	std::string m_strSession;
	std::string m_strTrdTpl;

	uint32_t	m_uVolScale;
	double		m_fPriceTick;
	uint32_t	m_uPrecision;

	ContractCategory	m_ccCategory;
	CoverMode			m_coverMode;
	PriceMode			m_priceMode;

	std::unordered_set<std::string> m_setCodes;
};

class WTSContractInfo :	public WTSObject
{
public:
	static WTSContractInfo* create(const char* code, const char* name, const char* exchg, const char* pid)
	{
		WTSContractInfo* ret = new WTSContractInfo;
		ret->m_strCode = code;
		ret->m_strName = name;
		ret->m_strProduct = pid;
		ret->m_strExchg = exchg;

		return ret;
	}


	void	setVolumnLimits(uint32_t maxMarketVol, uint32_t maxLimitVol)
	{
		m_maxMktQty = maxMarketVol;
		m_maxLmtQty = maxLimitVol;
	}

	const char* getCode()	const{return m_strCode.c_str();}
	const char* getExchg()	const{return m_strExchg.c_str();}
	const char* getName()	const{return m_strName.c_str();}
	const char* getProduct()	const{return m_strProduct.c_str();}

	uint32_t	getMaxMktVol() const{ return m_maxMktQty; }
	uint32_t	getMaxLmtVol() const{ return m_maxLmtQty; }


protected:
	WTSContractInfo(){}
	virtual ~WTSContractInfo(){}

private:
	std::string	m_strCode;
	std::string	m_strExchg;
	std::string	m_strName;
	std::string	m_strProduct;


	uint32_t	m_maxMktQty;
	uint32_t	m_maxLmtQty;
};


NS_OTP_END