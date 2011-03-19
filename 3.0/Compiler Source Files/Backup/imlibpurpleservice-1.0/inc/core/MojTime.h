/* ============================================================
 * Date  : Apr 15, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJTIME_H_
#define MOJTIME_H_

#include "core/MojCoreDefs.h"
#include "core/MojUtil.h"

class MojTime
{
public:
	static const MojInt64 UnitsPerSec = 1000000;
	static const MojInt64 UnitsPerMilli = 1000;
	static const MojInt64 UnitsPerDay = UnitsPerSec * 60 * 60 * 24;

	MojTime() : m_val(0) {}
	MojTime(const MojTime& time) : m_val(time.m_val) {}
	MojTime(MojInt64 microsecs) : m_val(microsecs) {}

	MojInt64 secs() const { return m_val / UnitsPerSec; }
	MojInt64 millisecs() const { return m_val / UnitsPerMilli; }
	MojInt64 microsecs() const { return m_val; }
	MojInt32 millisecsPart() const { return (MojInt32) (millisecs() % 1000); }
	MojInt32 microsecsPart() const { return (MojInt32) (m_val % UnitsPerSec); }

	void fromTimeval(const MojTimevalT* tv);
	void toTimeval(MojTimevalT* tvOut) const;

	void fromTimespec(const MojTimespecT* ts);
	void toTimespec(MojTimespecT* tsOut) const;

	const bool operator==(const MojTime& rhs) { return m_val == rhs.m_val; }
	const bool operator!=(const MojTime& rhs) { return m_val != rhs.m_val; }
	const bool operator<(const MojTime& rhs) { return m_val < rhs.m_val; }
	const bool operator<=(const MojTime& rhs) { return m_val <= rhs.m_val; }
	const bool operator>(const MojTime& rhs) { return m_val > rhs.m_val; }
	const bool operator>=(const MojTime& rhs) { return m_val >= rhs.m_val; }

	MojTime& operator=(const MojTime& rhs) { m_val = rhs.m_val; return *this; }
	MojTime& operator+=(const MojTime& rhs) { m_val += rhs.m_val; return *this; }
	MojTime& operator-=(const MojTime& rhs) { m_val -= rhs.m_val; return *this; }
	MojTime& operator*=(const MojTime& rhs) { m_val *= rhs.m_val; return *this; }
	MojTime& operator/=(const MojTime& rhs) { m_val /= rhs.m_val; return *this; }
	MojTime& operator%=(const MojTime& rhs) { m_val %= rhs.m_val; return *this; }
	MojTime& operator++() { ++m_val; return *this; }
	MojTime& operator--() { --m_val; return *this; }
	MojTime operator++(int) { return MojPostIncrement(*this); }
	MojTime operator--(int) { return MojPostDecrement(*this); }

	MojTime operator+(const MojTime& rhs) const { return MojTime(m_val + rhs.m_val); }
	MojTime operator-(const MojTime& rhs) const { return MojTime(m_val - rhs.m_val); }
	MojTime operator*(const MojTime& rhs) const { return MojTime(m_val * rhs.m_val); }
	MojTime operator/(const MojTime& rhs) const { return MojTime(m_val / rhs.m_val); }
	MojTime operator%(const MojTime& rhs) const { return MojTime(m_val % rhs.m_val); }

private:
	MojInt64 m_val;
};

// Alternate constructors
inline MojTime MojSecs(MojInt64 secs) { return MojTime(secs * MojTime::UnitsPerSec); }
inline MojTime MojMillisecs(MojInt64 millis) { return MojTime(millis * MojTime::UnitsPerMilli); }
inline MojTime MojMicrosecs(MojInt64 micros) { return MojTime(micros); }

#include "core/internal/MojTimeInternal.h"

#endif /* MOJTIME_H_ */
