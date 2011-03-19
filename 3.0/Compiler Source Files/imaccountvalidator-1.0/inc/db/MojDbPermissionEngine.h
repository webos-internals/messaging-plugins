/* ============================================================
 * Date  : Jun 22, 2010
 * Copyright 2010 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJDBPERMISSIONENGINE_H_
#define MOJDBPERMISSIONENGINE_H_

#include "db/MojDbDefs.h"
#include "db/MojDbPutHandler.h"
#include "core/MojHashMap.h"
#include "core/MojMap.h"
#include "core/MojString.h"
#include "core/MojThread.h"

class MojDbPermissionEngine : public MojDbPutHandler
{
public:
	enum Value {
		ValueUndefined,
		ValueAllow,
		ValueDeny
	};

	static const MojChar* const AllowKey;
	static const MojChar* const DenyKey;
	static const MojChar* const PermissionsKey;
	static const MojChar* const PermissionsEnabledKey;
	static const MojChar* const WildcardOperation;

	MojDbPermissionEngine();
	~MojDbPermissionEngine();

	virtual MojErr close();
	virtual MojErr put(MojObject& perm, MojDbReq& req, bool putObj = true);

	Value check(const MojChar* type, const MojChar* object, const MojChar* caller, const MojChar* op) const;
	bool enabled() const { return m_enabled; }

private:
	typedef MojMap<MojString, Value, const MojChar*> OperationMap;
	typedef MojMap<MojString, OperationMap, const MojChar*, LengthComp> CallerMap;
	typedef MojHashMap<MojString, CallerMap, const MojChar*> ObjectMap;
	typedef MojHashMap<MojString, ObjectMap, const MojChar*> TypeMap;

	virtual MojErr configure(const MojObject& conf, MojDbReq& req);
	static MojErr valueFromString(const MojString& str, Value& valOut);

	TypeMap m_types;
	bool m_enabled;

	static MojLogger s_log;
};

#endif /* MOJDBPERMISSIONENGINE_H_ */
