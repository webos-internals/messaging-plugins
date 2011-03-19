/* ============================================================
 * Date  : Jan 5, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJDBQUERY_H_
#define MOJDBQUERY_H_

#include "db/MojDbDefs.h"
#include "db/MojDbKey.h"
#include "core/MojHashMap.h"
#include "core/MojObject.h"
#include "core/MojSet.h"
#include "core/MojString.h"

class MojDbQuery
{
public:
	static const MojUInt32 LimitDefault = MojUInt32Max;

	typedef enum {
		OpNone,
		OpEq,
		OpNotEq,
		OpLessThan,
		OpLessThanEq,
		OpGreaterThan,
		OpGreaterThanEq,
		OpPrefix,
		OpSearch
	} CompOp;

	class WhereClause
	{
	public:
		WhereClause();

		MojErr setLower(CompOp op, const MojObject& val, MojDbCollationStrength coll);
		MojErr setUpper(CompOp op, const MojObject& val, MojDbCollationStrength coll);

		MojDbCollationStrength collation() const { return m_collation; }
		CompOp lowerOp() const { return m_lowerOp; }
		CompOp upperOp() const { return m_upperOp; }
		const MojObject& lowerVal() const { return m_lowerVal; }
		const MojObject& upperVal() const { return m_upperVal; }
		bool definesOrder() const { return m_lowerOp != OpEq || m_lowerVal.type() == MojObject::TypeArray; }
		bool operator==(const WhereClause& rhs) const;

	private:
		MojErr collation(MojDbCollationStrength collation);

		CompOp m_lowerOp;
		CompOp m_upperOp;
		MojObject m_lowerVal;
		MojObject m_upperVal;
		MojDbCollationStrength m_collation;
	};

	class Page
	{
	public:
		void clear() { m_key.clear(); }
		MojErr fromObject(const MojObject& obj);
		MojErr toObject(MojObject& objOut) const;
		bool empty() const { return m_key.empty(); }

		const MojDbKey& key() const { return m_key; }
		MojDbKey& key() { return m_key; }

	private:
		MojDbKey m_key;
	};

	typedef MojHashMap<MojString, WhereClause, const MojChar*> WhereMap;
	typedef MojSet<MojString> StringSet;

	static const MojChar* const SelectKey;
	static const MojChar* const FromKey;
	static const MojChar* const WhereKey;
	static const MojChar* const FilterKey;
	static const MojChar* const OrderByKey;
	static const MojChar* const DescKey;
	static const MojChar* const IncludeDeletedKey;
	static const MojChar* const LimitKey;
	static const MojChar* const PageKey;
	static const MojChar* const PropKey;
	static const MojChar* const OpKey;
	static const MojChar* const ValKey;
	static const MojChar* const CollateKey;
	static const MojChar* const DelKey;
	static const MojUInt32 MaxQueryLimit = 500;

	MojDbQuery();
	~MojDbQuery();

	MojErr fromObject(const MojObject& obj);
	MojErr toObject(MojObject& objOut) const;
	MojErr toObject(MojObjectVisitor& visitor) const;

	void clear();
	MojErr select(const MojChar* propName);
	MojErr from(const MojChar* type);
	MojErr where(const MojChar* propName, CompOp op, const MojObject& val, MojDbCollationStrength coll = MojDbCollationInvalid);
	MojErr filter(const MojChar* propName, CompOp op, const MojObject& val);
	MojErr order(const MojChar* propName);
	MojErr includeDeleted(bool val = true);
	void desc(bool val) { m_desc = val; }
	void limit(MojUInt32 numResults) { m_limit = numResults; }
	void page(const Page& page) { m_page = page; }

	MojErr validate() const;
	MojErr validateFind() const;
	const StringSet& select() const { return m_selectProps; }
	const MojString& from() const { return m_fromType; }
	const WhereMap& where() const { return m_whereClauses; }
	const WhereMap& filter() const { return m_filterClauses; }
	const MojString& order() const { return m_orderProp; }
	const Page& page() const { return m_page; }
	bool desc() const { return m_desc; }
	MojUInt32 limit() const { return m_limit; }
	bool operator==(const MojDbQuery& rhs) const;

	static MojErr stringToOp(const MojChar* str, CompOp& opOut);

private:
	friend class MojDbQueryPlan;

	struct StrOp
	{
		const MojChar* const m_str;
		const CompOp m_op;
	};
	static const StrOp s_ops[];

	void init();
	static MojErr addClauses(WhereMap& map, const MojObject& array);
	static MojErr addClause(WhereMap& map, const MojChar* propName, CompOp op, const MojObject& val, MojDbCollationStrength coll);
	static MojErr createClause(WhereMap& map, const MojChar* propName, CompOp op, MojObject val, MojDbCollationStrength coll);
	static MojErr updateClause(WhereClause& clause, CompOp op, const MojObject& val, MojDbCollationStrength coll);
	static const MojChar* opToString(CompOp op);
	static MojErr appendClauses(MojObjectVisitor& visitor, const MojChar* propName, const WhereMap& map);
	static MojErr appendClause(MojObjectVisitor& visitor, const MojChar* propName, CompOp op,
							  const MojObject& val, MojDbCollationStrength coll);

	MojString m_fromType;
	StringSet m_selectProps;
	WhereMap m_whereClauses;
	WhereMap m_filterClauses;
	Page m_page;
	MojString m_orderProp;
	MojUInt32 m_limit;
	bool m_desc;

	static MojLogger s_log;
};

#endif /* MOJDBQUERY_H_ */
