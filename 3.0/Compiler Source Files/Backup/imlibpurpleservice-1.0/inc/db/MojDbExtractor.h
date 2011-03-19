/* ============================================================
 * Date  : Apr 02, 2010
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJDBEXTRACTOR_H_
#define MOJDBEXTRACTOR_H_

#include "db/MojDbDefs.h"
#include "db/MojDbKey.h"
#include "db/MojDbTextCollator.h"
#include "db/MojDbTextTokenizer.h"
#include "core/MojObject.h"
#include "core/MojSet.h"

class MojDbExtractor : public MojRefCounted
{
public:
	static const MojChar* const CollateKey;
	static const MojChar* const NameKey;

	typedef MojSet<MojDbKey> KeySet;

	MojDbExtractor() : m_collation(MojDbCollationInvalid) {}
	virtual ~MojDbExtractor() {}
	virtual MojErr fromObject(const MojObject& obj, const MojChar* locale) = 0;
	virtual MojErr updateLocale(const MojChar* locale) = 0;
	virtual MojErr vals(const MojObject& obj, KeySet& valsOut) const = 0;

	const MojString& name() const { return m_name; }
	MojDbCollationStrength collation() const { return m_collation; }

protected:
	MojString m_name;
	MojDbCollationStrength m_collation;
};

class MojDbPropExtractor : public MojDbExtractor
{
public:
	static const MojChar* const AllKey;
	static const MojChar* const DefaultKey;
	static const MojChar* const PrimaryKey;
	static const MojChar* const SecondaryKey;
	static const MojChar* const TertiaryKey;
	static const MojChar* const TokenizeKey;

	MojDbPropExtractor();
	void collator(MojDbTextCollator* collator) { m_collator.reset(collator); }
	MojErr prop(const MojString& name);
	virtual MojErr fromObject(const MojObject& obj, const MojChar* locale);
	virtual MojErr updateLocale(const MojChar* locale);
	virtual MojErr vals(const MojObject& obj, KeySet& valsOut) const { return valsImpl(obj, valsOut, 0); }

private:
	friend class MojDbMultiExtractor;
	typedef MojVector<MojString> StringVec;

	static const MojChar PropComponentSeparator;
	static const MojChar* const WildcardKey;

	MojErr fromObjectImpl(const MojObject& obj, const MojDbPropExtractor& defaultConfig, const MojChar* locale);
	MojErr valsImpl(const MojObject& obj, KeySet& valsOut, MojSize idx) const;
	MojErr handleVal(const MojObject& val, KeySet& valsOut, MojSize idx) const;

	KeySet m_default;
	StringVec m_prop;
	MojRefCountedPtr<MojDbTextTokenizer> m_tokenizer;
	MojRefCountedPtr<MojDbTextCollator> m_collator;
};

class MojDbMultiExtractor : public MojDbExtractor
{
public:
	static const MojChar* const IncludeKey; // include

	virtual MojErr fromObject(const MojObject& obj, const MojChar* locale);
	virtual MojErr updateLocale(const MojChar* locale);
	virtual MojErr vals(const MojObject& obj, KeySet& valsOut) const;

private:
	typedef MojVector<MojRefCountedPtr<MojDbExtractor> > ExtractorVec;

	ExtractorVec m_extractors;
};

#endif /* MOJDBEXTRACTOR_H_ */
