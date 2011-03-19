/* ============================================================
 * Date  : Mar 23, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJDECIMAL_H_
#define MOJDECIMAL_H_

#include "core/MojCoreDefs.h"
#include "core/MojHasher.h"

class MojDecimal
{
public:
	static const MojInt32 Numerator = 1000000;
	static const MojSize Precision = 6;
	static const MojSize MaxStringSize = 24;
	static const MojInt64 MagnitudeMax = MojInt64Max / Numerator - Numerator;
	static const MojInt64 MagnitudeMin = MojInt64Min / Numerator + Numerator;
	static const MojUInt32 FractionMax = Numerator - 1;
	static const MojUInt32 FractionMin = 0;

	MojDecimal() : m_rep(0) {}
	MojDecimal(const MojDecimal& dec) : m_rep(dec.m_rep) {}
	explicit MojDecimal(MojInt64 magnitude, MojUInt32 fraction = 0) { assign(magnitude, fraction); }
	explicit MojDecimal(MojDouble d) { assign(d); }

	MojInt64 magnitude() const { return m_rep / Numerator; }
	MojUInt32 fraction() const;
	MojDouble floatValue() const;
	MojInt64 rep() const { return m_rep; }
	MojErr stringValue(MojChar* buf, MojSize size) const;

	MojErr assign(const MojChar* str);
	void assign(MojInt64 magnitude, MojUInt32 fraction);
	void assign(MojDouble d) { m_rep = (MojInt64) (d * ((MojDouble) Numerator)); }
	void assignRep(MojInt64 rep) { m_rep = rep; }

	MojDecimal& operator=(const MojDecimal& rhs) { m_rep = rhs.m_rep; return *this; }
	bool operator==(const MojDecimal& rhs) const { return m_rep == rhs.m_rep; }
	bool operator!=(const MojDecimal& rhs) const { return m_rep != rhs.m_rep; }
	bool operator<(const MojDecimal& rhs) const { return m_rep < rhs.m_rep; }
	bool operator<=(const MojDecimal& rhs) const { return m_rep <= rhs.m_rep; }
	bool operator>(const MojDecimal& rhs) const { return m_rep > rhs.m_rep; }
	bool operator>=(const MojDecimal& rhs) const { return m_rep >= rhs.m_rep; }

private:
	MojInt64 m_rep;
};

template<>
struct MojHasher<MojDecimal>
{
	MojSize operator()(const MojDecimal& val)
	{
		MojInt64 rep = val.rep();
		return MojHash(&rep, sizeof(rep));
	}
};

#endif /* MOJDECIMAL_H_ */
