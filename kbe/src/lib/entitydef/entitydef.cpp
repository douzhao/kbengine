/*
This source file is part of KBEngine
For the latest info, see http://www.kbengine.org/

Copyright (c) 2008-2012 KBEngine.

KBEngine is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

KBEngine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
 
You should have received a copy of the GNU Lesser General Public License
along with KBEngine.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "entitydef.hpp"
#include "datatypes.hpp"
#include "common.hpp"
#include "cstdkbe/smartpointer.hpp"
#include "entitydef/entity_mailbox.hpp"

namespace KBEngine{
std::vector<ScriptModule *>	EntityDef::__scriptModules;
std::map<std::string, ENTITY_SCRIPT_UID> EntityDef::__scriptTypeMappingUType;
COMPONENT_TYPE EntityDef::__loadComponentType;

// 方法产生时自动产生utype用的
ENTITY_METHOD_UID g_methodUtypeAuto = 1;
std::vector<ENTITY_METHOD_UID> g_methodCusUtypes;

//-------------------------------------------------------------------------------------
ScriptModule::ScriptModule(std::string name):
scriptType_(NULL),
uType_(0),
hasCell_(false),
hasBase_(false),
hasClient_(false),
name_(name)
{
	detailLevel_.level[DETAIL_LEVEL_NEAR] = new DetailLevel::Level();
	detailLevel_.level[DETAIL_LEVEL_MEDIUM] = new DetailLevel::Level();
	detailLevel_.level[DETAIL_LEVEL_FAR] = new DetailLevel::Level();
	detailLevel_.level[DETAIL_LEVEL_UNKNOW] = new DetailLevel::Level();
	
	detailLevel_.level[DETAIL_LEVEL_NEAR]->radius = CELL_BORDER_WIDTH;
	detailLevel_.level[DETAIL_LEVEL_NEAR]->hyst = 0.0f;
	
	detailLevel_.level[DETAIL_LEVEL_MEDIUM]->radius = 0.0f;
	detailLevel_.level[DETAIL_LEVEL_MEDIUM]->hyst = 0.0f;
	
	detailLevel_.level[DETAIL_LEVEL_FAR]->radius = 0.0f;
	detailLevel_.level[DETAIL_LEVEL_FAR]->hyst = 0.0f;
	
	detailLevel_.level[DETAIL_LEVEL_UNKNOW]->radius = 999999999.0f;
	detailLevel_.level[DETAIL_LEVEL_UNKNOW]->hyst = 0.0f;
}

//-------------------------------------------------------------------------------------
ScriptModule::~ScriptModule()
{
	S_RELEASE(scriptType_);
	PROPERTYDESCRIPTION_MAP::iterator iter1 = cellPropertyDescr_.begin();
	for(; iter1 != cellPropertyDescr_.end(); iter1++)
		iter1->second->decRef();

	iter1 = basePropertyDescr_.begin();
	for(; iter1 != basePropertyDescr_.end(); iter1++)
		iter1->second->decRef();

	iter1 = clientPropertyDescr_.begin();
	for(; iter1 != clientPropertyDescr_.end(); iter1++)
		iter1->second->decRef();

	METHODDESCRIPTION_MAP::iterator iter2 = methodCellDescr_.begin();
	for(; iter2 != methodCellDescr_.end(); iter2++)
		SAFE_RELEASE(iter2->second);
		
	METHODDESCRIPTION_MAP::iterator iter3 = methodBaseDescr_.begin();
	for(; iter3 != methodBaseDescr_.end(); iter3++)
		SAFE_RELEASE(iter3->second);
		
	METHODDESCRIPTION_MAP::iterator iter4 = methodClientDescr_.begin();
	for(; iter4 != methodClientDescr_.end(); iter4++)
		SAFE_RELEASE(iter4->second);
}

//-------------------------------------------------------------------------------------
ENTITY_SCRIPT_UID ScriptModule::getUType(void)
{
	return uType_;
}

//-------------------------------------------------------------------------------------
PyTypeObject* ScriptModule::getScriptType(void)
{
	return scriptType_;
}

//-------------------------------------------------------------------------------------
PyObject* ScriptModule::createObject(void)
{
	PyObject * pObject = PyType_GenericAlloc(scriptType_, 0);
	if (pObject == NULL)
	{
		PyErr_Print();
		ERROR_MSG("ScriptModule::createObject: GenericAlloc is failed.\n");
	}
	return pObject;
}

//-------------------------------------------------------------------------------------
PyObject* ScriptModule::getInitDict(void)
{
	return NULL;
}

//-------------------------------------------------------------------------------------
bool ScriptModule::addPropertyDescription(const char* attrName, 
										  PropertyDescription* propertyDescription, COMPONENT_TYPE componentType)
{
	PropertyDescription* f_propertyDescription = NULL;
	PROPERTYDESCRIPTION_MAP*  propertyDescr;
	PROPERTYDESCRIPTION_UIDMAP*  propertyDescr_uidmap;

	switch(componentType)
	{
	case CELLAPP_TYPE:
			f_propertyDescription = findCellPropertyDescription(attrName);
			propertyDescr = &getCellPropertyDescriptions();
			propertyDescr_uidmap = &getCellPropertyDescriptions_uidmap();
			
			// 判断他们是什么级别的属性， 将其保存到对应detailLevel的地方
			if((propertyDescription->getFlags() & ENTITY_CLIENT_DATA_FLAGS) > 0){
				cellDetailLevelPropertyDescrs_[propertyDescription->getDetailLevel()][attrName] = propertyDescription;
			}
			break;
	case BASEAPP_TYPE:
			f_propertyDescription = findBasePropertyDescription(attrName);
			propertyDescr = &getBasePropertyDescriptions();
			propertyDescr_uidmap = &getBasePropertyDescriptions_uidmap();
			break;
	default:
			f_propertyDescription = findClientPropertyDescription(attrName);
			propertyDescr = &getClientPropertyDescriptions();
			propertyDescr_uidmap = &getClientPropertyDescriptions_uidmap();
			break;
	};

	if(f_propertyDescription)
	{
		ERROR_MSG("ScriptModule::addPropertyDescription: [%s] is exist! componentType=%d.\n", attrName, componentType);
		return false;
	}

	(*propertyDescr)[attrName] = propertyDescription;
	(*propertyDescr_uidmap)[propertyDescription->getUType()] = propertyDescription;
	propertyDescription->incRef();
	return true;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findCellPropertyDescription(const char* attrName)
{
	PROPERTYDESCRIPTION_MAP::iterator iter = cellPropertyDescr_.find(attrName);
	if(iter == cellPropertyDescr_.end())
	{
		//ERROR_MSG("ScriptModule::findCellPropertyDescription: [%s] not found!\n", attrName);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findBasePropertyDescription(const char* attrName)
{
	PROPERTYDESCRIPTION_MAP::iterator iter = basePropertyDescr_.find(attrName);
	if(iter == basePropertyDescr_.end())
	{
		//ERROR_MSG("ScriptModule::findBasePropertyDescription: [%s] not found!\n", attrName);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findClientPropertyDescription(const char* attrName)
{
	PROPERTYDESCRIPTION_MAP::iterator iter = clientPropertyDescr_.find(attrName);
	if(iter == clientPropertyDescr_.end())
	{
		//ERROR_MSG("ScriptModule::findClientPropertyDescription: [%s] not found!\n", attrName);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findCellPropertyDescription(ENTITY_PROPERTY_UID utype)
{
	PROPERTYDESCRIPTION_UIDMAP::iterator iter = cellPropertyDescr_uidmap_.find(utype);

	if(iter == cellPropertyDescr_uidmap_.end())
	{
		//ERROR_MSG("ScriptModule::findCellPropertyDescription: [%ld] not found!\n", utype);
		return NULL;
	}

	return iter->second;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findBasePropertyDescription(ENTITY_PROPERTY_UID utype)
{
	PROPERTYDESCRIPTION_UIDMAP::iterator iter = basePropertyDescr_uidmap_.find(utype);

	if(iter == basePropertyDescr_uidmap_.end())
	{
		//ERROR_MSG("ScriptModule::findBasePropertyDescription: [%ld] not found!\n", utype);
		return NULL;
	}

	return iter->second;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findClientPropertyDescription(ENTITY_PROPERTY_UID utype)
{
	PROPERTYDESCRIPTION_UIDMAP::iterator iter = clientPropertyDescr_uidmap_.find(utype);

	if(iter == clientPropertyDescr_uidmap_.end())
	{
		//ERROR_MSG("ScriptModule::findClientPropertyDescription: [%ld] not found!\n", utype);
		return NULL;
	}

	return iter->second;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findPropertyDescription(const char* attrName, COMPONENT_TYPE componentType)
{
	switch(componentType)
	{
	case CELLAPP_TYPE:
			return findCellPropertyDescription(attrName);
			break;
	case BASEAPP_TYPE:
			return findBasePropertyDescription(attrName);
			break;
	default:
			return findClientPropertyDescription(attrName);
			break;
	};

	return NULL;
}

//-------------------------------------------------------------------------------------
PropertyDescription* ScriptModule::findPropertyDescription(ENTITY_PROPERTY_UID utype, COMPONENT_TYPE componentType)
{
	switch(componentType)
	{
	case CELLAPP_TYPE:
			return findCellPropertyDescription(utype);
			break;
	case BASEAPP_TYPE:
			return findBasePropertyDescription(utype);
			break;
	default:
			return findClientPropertyDescription(utype);
			break;
	};

	return NULL;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findMethodDescription(const char* attrName, COMPONENT_TYPE componentType)
{
	switch(componentType)
	{
	case CELLAPP_TYPE:
			return findCellMethodDescription(attrName);
			break;
	case BASEAPP_TYPE:
			return findBaseMethodDescription(attrName);
			break;
	default:
			return findClientMethodDescription(attrName);
			break;
	};

	return NULL;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findMethodDescription(ENTITY_METHOD_UID utype, COMPONENT_TYPE componentType)
{
	switch(componentType)
	{
	case CELLAPP_TYPE:
			return findCellMethodDescription(utype);
			break;
	case BASEAPP_TYPE:
			return findBaseMethodDescription(utype);
			break;
	default:
			return findClientMethodDescription(utype);
			break;
	};

	return NULL;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findCellMethodDescription(const char* attrName)
{
	METHODDESCRIPTION_MAP::iterator iter = methodCellDescr_.find(attrName);
	if(iter == methodCellDescr_.end())
	{
		//ERROR_MSG("ScriptModule::findCellMethodDescription: [%s] not found!\n", attrName);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findCellMethodDescription(ENTITY_METHOD_UID utype)
{
	METHODDESCRIPTION_UIDMAP::iterator iter = methodCellDescr_uidmap_.find(utype);
	if(iter == methodCellDescr_uidmap_.end())
	{
		//ERROR_MSG("ScriptModule::findCellMethodDescription: [%ld] not found!\n", utype);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
bool ScriptModule::addCellMethodDescription(const char* attrName, MethodDescription* methodDescription)
{
	MethodDescription* f_methodDescription = findCellMethodDescription(attrName);
	if(f_methodDescription)
	{
		ERROR_MSG("ScriptModule::addCellMethodDescription: [%s] is exist!\n", attrName);
		return false;
	}

	methodCellDescr_[attrName] = methodDescription;
	methodCellDescr_uidmap_[methodDescription->getUType()] = methodDescription;
	return true;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findBaseMethodDescription(const char* attrName)
{
	METHODDESCRIPTION_MAP::iterator iter = methodBaseDescr_.find(attrName);
	if(iter == methodBaseDescr_.end())
	{
		//ERROR_MSG("ScriptModule::findBaseMethodDescription: [%s] not found!\n", attrName);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findBaseMethodDescription(ENTITY_METHOD_UID utype)
{
	METHODDESCRIPTION_UIDMAP::iterator iter = methodBaseDescr_uidmap_.find(utype);
	if(iter == methodBaseDescr_uidmap_.end())
	{
		//ERROR_MSG("ScriptModule::findBaseMethodDescription: [%ld] not found!\n", utype);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
bool ScriptModule::addBaseMethodDescription(const char* attrName, MethodDescription* methodDescription)
{
	MethodDescription* f_methodDescription = findBaseMethodDescription(attrName);
	if(f_methodDescription)
	{
		ERROR_MSG("ScriptModule::addBaseMethodDescription: [%s] is exist!\n", attrName);
		return false;
	}

	methodBaseDescr_[attrName] = methodDescription;
	methodBaseDescr_uidmap_[methodDescription->getUType()] = methodDescription;
	return true;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findClientMethodDescription(const char* attrName)
{
	METHODDESCRIPTION_MAP::iterator iter = methodClientDescr_.find(attrName);
	if(iter == methodClientDescr_.end())
	{
		//ERROR_MSG("ScriptModule::findClientMethodDescription: [%s] not found!\n", attrName);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
MethodDescription* ScriptModule::findClientMethodDescription(ENTITY_METHOD_UID utype)
{
	METHODDESCRIPTION_UIDMAP::iterator iter = methodClientDescr_uidmap_.find(utype);
	if(iter == methodClientDescr_uidmap_.end())
	{
		//ERROR_MSG("ScriptModule::findClientMethodDescription: [%ld] not found!\n", utype);
		return NULL;
	}
	return iter->second;
}

//-------------------------------------------------------------------------------------
bool ScriptModule::addClientMethodDescription(const char* attrName, MethodDescription* methodDescription)
{
	MethodDescription* f_methodDescription = findClientMethodDescription(attrName);
	if(f_methodDescription)
	{
		ERROR_MSG("ScriptModule::addClientMethodDescription: [%s] is exist!\n", attrName);
		return false;
	}

	methodClientDescr_[attrName] = methodDescription;
	methodClientDescr_uidmap_[methodDescription->getUType()] = methodDescription;
	return true;
}

//-------------------------------------------------------------------------------------
ScriptModule::PROPERTYDESCRIPTION_MAP& ScriptModule::getPropertyDescrs()										
{																										
	ScriptModule::PROPERTYDESCRIPTION_MAP* lpPropertyDescrs = NULL;										
																										
	switch(g_componentType)																				
	{																									
		case CELLAPP_TYPE:																				
			lpPropertyDescrs = &getCellPropertyDescriptions();							
			break;																						
		case BASEAPP_TYPE:																				
			lpPropertyDescrs = &getBasePropertyDescriptions();							
			break;																						
		default:																						
			lpPropertyDescrs = &getClientPropertyDescriptions();							
			break;																						
	};																									
																									
	return *lpPropertyDescrs;																			
}																										

//-------------------------------------------------------------------------------------
EntityDef::EntityDef()
{
}

//-------------------------------------------------------------------------------------
EntityDef::~EntityDef()
{	
}

//-------------------------------------------------------------------------------------
bool EntityDef::finish(void)
{
	std::vector<ScriptModule *>::iterator iter = EntityDef::__scriptModules.begin();
	for(;iter != EntityDef::__scriptModules.end(); iter++)
		delete *iter;

	EntityDef::__scriptModules.clear();
	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::initialize(const std::string entitiesPath, std::vector<PyTypeObject*>& scriptBaseTypes, COMPONENT_TYPE loadComponentType)
{
	__loadComponentType = loadComponentType;
	g_entityFlagMapping["CELL_PUBLIC"]							= ED_FLAG_CELL_PUBLIC;
	g_entityFlagMapping["CELL_PRIVATE"]							= ED_FLAG_CELL_PRIVATE;
	g_entityFlagMapping["ALL_CLIENTS"]							= ED_FLAG_ALL_CLIENTS;
	g_entityFlagMapping["CELL_PUBLIC_AND_OWN"]					= ED_FLAG_CELL_PUBLIC_AND_OWN;
	g_entityFlagMapping["BASE_AND_CLIENT"]						= ED_FLAG_BASE_AND_CLIENT;
	g_entityFlagMapping["BASE"]									= ED_FLAG_BASE;
	g_entityFlagMapping["OTHER_CLIENTS"]						= ED_FLAG_OTHER_CLIENTS;

	std::string entitiesFile = entitiesPath + "entities.xml";
	std::string defFilePath = entitiesPath + "entity_defs/";
	ENTITY_SCRIPT_UID utype = 0;
	
	// 打开这个entities.xml文件
	SmartPointer<XmlPlus> xml(new XmlPlus());
	if(!xml.get()->openSection(entitiesFile.c_str()))
		return false;
	
	// 获得entities.xml根节点, 如果没有定义一个entity那么直接返回true
	TiXmlNode* node = xml.get()->getRootNode();
	if(node == NULL)
		return true;

	// 开始遍历所有的entity节点
	XML_FOR_BEGIN(node)
	{
		std::string moduleName = xml.get()->getKey(node);
		__scriptTypeMappingUType[moduleName] = utype;
		ScriptModule* scriptModule = new ScriptModule(moduleName);
		scriptModule->setUType(++utype);
		EntityDef::__scriptModules.push_back(scriptModule);

		std::string deffile = defFilePath + moduleName + ".def";
		SmartPointer<XmlPlus> defxml(new XmlPlus());
		if(!defxml.get()->openSection(deffile.c_str()))
			return false;

		TiXmlNode* defNode = defxml.get()->getRootNode();
		// 加载def文件中的定义
		if(!loadDefInfo(defFilePath, moduleName, defxml.get(), defNode, scriptModule))
		{
			ERROR_MSG("EntityDef::initialize: failed to load entity:%s parentClass.\n", moduleName.c_str());
			return false;
		}
		
		// 尝试在主entity文件中加载detailLevel数据
		if(!loadDetailLevelInfo(defFilePath, moduleName, defxml.get(), defNode, scriptModule))
		{
			ERROR_MSG("EntityDef::initialize: failed to load entity:%s DetailLevelInfo.\n", moduleName.c_str());
			return false;
		}
	}
	XML_FOR_END(node);

	if(loadComponentType == DBMGR_TYPE)
		return true;

	return loadAllScriptModule(entitiesPath, scriptBaseTypes);
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadDefInfo(std::string& defFilePath, std::string& moduleName, XmlPlus* defxml, TiXmlNode* defNode, ScriptModule* scriptModule)
{
	if(!loadAllDefDescription(moduleName, defxml, defNode, scriptModule))
	{
		ERROR_MSG("EntityDef::loadDefInfo: failed to loadAllDefDescription(), entity:%s\n", moduleName.c_str());
		return false;
	}
	
	// 遍历所有的interface， 并将他们的方法和属性加入到模块中
	if(!loadInterfaces(defFilePath, moduleName, defxml, defNode, scriptModule))
	{
		ERROR_MSG("EntityDef::loadDefInfo: failed to load entity:%s interface.\n", moduleName.c_str());
		return false;
	}
	
	// 加载父类所有的内容
	if(!loadParentClass(defFilePath, moduleName, defxml, defNode, scriptModule))
	{
		ERROR_MSG("EntityDef::loadDefInfo: failed to load entity:%s parentClass.\n", moduleName.c_str());
		return false;
	}

	// 尝试加载detailLevel数据
	if(!loadDetailLevelInfo(defFilePath, moduleName, defxml, defNode, scriptModule))
	{
		ERROR_MSG("EntityDef::loadDefInfo: failed to load entity:%s DetailLevelInfo.\n", moduleName.c_str());
		return false;
	}
		
	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadDetailLevelInfo(std::string& defFilePath, std::string& moduleName, XmlPlus* defxml, TiXmlNode* defNode, ScriptModule* scriptModule)
{
	TiXmlNode* detailLevelNode = defxml->enterNode(defNode, "DetailLevels");
	if(detailLevelNode == NULL)
		return true;

	DetailLevel& dlInfo = scriptModule->getDetailLevel();
	
	TiXmlNode* node = defxml->enterNode(detailLevelNode, "NEAR");
	TiXmlNode* radiusNode = defxml->enterNode(node, "radius");
	TiXmlNode* hystNode = defxml->enterNode(node, "hyst");
	if(node == NULL || radiusNode == NULL || hystNode == NULL) 
	{
		ERROR_MSG("EntityDef::loadDetailLevelInfo: failed to load entity:%s NEAR-DetailLevelInfo.\n", moduleName.c_str());
		return false;
	}
	
	dlInfo.level[DETAIL_LEVEL_NEAR]->radius = (float)defxml->getValFloat(radiusNode);
	dlInfo.level[DETAIL_LEVEL_NEAR]->hyst = (float)defxml->getValFloat(hystNode);
	
	node = defxml->enterNode(detailLevelNode, "MEDIUM");
	radiusNode = defxml->enterNode(node, "radius");
	hystNode = defxml->enterNode(node, "hyst");
	if(node == NULL || radiusNode == NULL || hystNode == NULL) 
	{
		ERROR_MSG("EntityDef::loadDetailLevelInfo: failed to load entity:%s MEDIUM-DetailLevelInfo.\n", moduleName.c_str());
		return false;
	}
	
	dlInfo.level[DETAIL_LEVEL_MEDIUM]->radius = (float)defxml->getValFloat(radiusNode);
	dlInfo.level[DETAIL_LEVEL_MEDIUM]->radius += dlInfo.level[DETAIL_LEVEL_NEAR]->radius + dlInfo.level[DETAIL_LEVEL_NEAR]->hyst;
	dlInfo.level[DETAIL_LEVEL_MEDIUM]->hyst = (float)defxml->getValFloat(hystNode);
	
	node = defxml->enterNode(detailLevelNode, "FAR");
	radiusNode = defxml->enterNode(node, "radius");
	hystNode = defxml->enterNode(node, "hyst");
	if(node == NULL || radiusNode == NULL || hystNode == NULL) 
	{
		ERROR_MSG("EntityDef::loadDetailLevelInfo: failed to load entity:%s FAR-DetailLevelInfo.\n", moduleName.c_str());
		return false;
	}
	
	dlInfo.level[DETAIL_LEVEL_FAR]->radius = (float)defxml->getValFloat(radiusNode);
	dlInfo.level[DETAIL_LEVEL_FAR]->radius += dlInfo.level[DETAIL_LEVEL_MEDIUM]->radius + dlInfo.level[DETAIL_LEVEL_MEDIUM]->hyst;
	dlInfo.level[DETAIL_LEVEL_FAR]->hyst = (float)defxml->getValFloat(hystNode);
	return true;

}

//-------------------------------------------------------------------------------------
bool EntityDef::loadInterfaces(std::string& defFilePath, std::string& moduleName, XmlPlus* defxml, TiXmlNode* defNode, ScriptModule* scriptModule)
{
	TiXmlNode* implementsNode = defxml->enterNode(defNode, "Implements");
	if(implementsNode == NULL)
		return true;

	XML_FOR_BEGIN(implementsNode)
	{
		TiXmlNode* interfaceNode = defxml->enterNode(implementsNode, "Interface");
		std::string interfaceName = defxml->getKey(interfaceNode);
		std::string interfacefile = defFilePath + "interfaces/" + interfaceName + ".def";
		SmartPointer<XmlPlus> interfaceXml(new XmlPlus());
		if(!interfaceXml.get()->openSection(interfacefile.c_str()))
			return false;

		TiXmlNode* interfaceRootNode = interfaceXml.get()->getRootNode();
		if(!loadAllDefDescription(moduleName, interfaceXml.get(), interfaceRootNode, scriptModule))
		{
			ERROR_MSG("EntityDef::initialize: interface[%s] is error!\n", interfaceName.c_str());
			return false;
		}

		// 尝试加载detailLevel数据
		if(!loadDetailLevelInfo(defFilePath, moduleName, interfaceXml.get(), interfaceRootNode, scriptModule))
		{
			ERROR_MSG("EntityDef::loadInterfaces: failed to load entity:%s DetailLevelInfo.\n", moduleName.c_str());
			return false;
		}
	}
	XML_FOR_END(implementsNode);

	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadParentClass(std::string& defFilePath, std::string& moduleName, XmlPlus* defxml, TiXmlNode* defNode, ScriptModule* scriptModule)
{
	TiXmlNode* parentClassNode = defxml->enterNode(defNode, "Parent");
	if(parentClassNode == NULL)
		return true;
	
	std::string parentClassName = defxml->getKey(parentClassNode);
	std::string parentClassfile = defFilePath + parentClassName + ".def";
	
	SmartPointer<XmlPlus> parentClassXml(new XmlPlus());
	if(!parentClassXml.get()->openSection(parentClassfile.c_str()))
		return false;
	
	TiXmlNode* parentClassdefNode = parentClassXml.get()->getRootNode();

	// 加载def文件中的定义
	if(!loadDefInfo(defFilePath, parentClassName, parentClassXml.get(), parentClassdefNode, scriptModule))
	{
		ERROR_MSG("EntityDef::loadParentClass: failed to load entity:%s parentClass.\n", moduleName.c_str());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadAllDefDescription(std::string& moduleName, XmlPlus* defxml, TiXmlNode* defNode, ScriptModule* scriptModule)
{
	// 加载属性描述
	if(!loadDefPropertys(moduleName, defxml, defxml->enterNode(defNode, "Properties"), scriptModule))
		return false;
	
	if(defxml->hasNode(defNode, "CellMethods"))
		scriptModule->setCell(true);
	
	if(defxml->hasNode(defNode, "BaseMethods"))
		scriptModule->setBase(true);
	
	if(defxml->hasNode(defNode, "ClientMethods"))
		scriptModule->setClient(true);

	// 加载cell方法描述
	if(!loadDefCellMethods(moduleName, defxml, defxml->enterNode(defNode, "CellMethods"), scriptModule)){
		ERROR_MSG("EntityDef::loadAllDefDescription:loadDefCellMethods[%s] is failed!\n", moduleName.c_str());
		return false;
	}

	// 加载base方法描述
	if(!loadDefBaseMethods(moduleName, defxml, defxml->enterNode(defNode, "BaseMethods"), scriptModule)){
		ERROR_MSG("EntityDef::loadAllDefDescription:loadDefBaseMethods[%s] is failed!\n", moduleName.c_str());
		return false;
	}

	// 加载client方法描述
	if(!loadDefClientMethods(moduleName, defxml, defxml->enterNode(defNode, "ClientMethods"), scriptModule)){
		ERROR_MSG("EntityDef::loadAllDefDescription:loadDefClientMethods[%s] is failed!\n", moduleName.c_str());
		return false;
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadDefPropertys(std::string& moduleName, XmlPlus* xml, TiXmlNode* defPropertyNode, ScriptModule* scriptModule)
{
	if(defPropertyNode)
	{
		XML_FOR_BEGIN(defPropertyNode)
		{
			ENTITY_PROPERTY_UID			futype = 0;
			uint32						flags = 0;
			int32						hasBaseFlags = 0;
			int32						hasCellFlags = 0;
			int32						hasClientFlags = 0;
			DataType*					dataType = NULL;
			bool						isPersistent = false;
			bool						isIdentifier = false;													// 是否是一个索引键
			uint32						databaseLength = 0;														// 这个属性在数据库中的长度
			uint8						detailLevel = DETAIL_LEVEL_NEAR;
			std::string					detailLevelStr = "NEAR";
			std::string					strType;
			std::string					strisPersistent;
			std::string					strFlags;
			std::string					strIdentifierNode;
			std::string					defaultStr;
			std::string					name = "";

			name = xml->getKey(defPropertyNode);
			TiXmlNode* flagsNode = xml->enterNode(defPropertyNode->FirstChild(), "Flags");
			if(flagsNode)
			{
				strFlags = xml->getValStr(flagsNode);
				std::transform(strFlags.begin(), strFlags.end(), strFlags.begin(), toupper);					// 转换为大写
				ENTITYFLAGMAP::iterator iter = g_entityFlagMapping.find(strFlags.c_str());
				if(iter == g_entityFlagMapping.end())
				{
					ERROR_MSG("EntityDef::loadDefPropertys: can't fount entity the flags[%s] in %s.\n", 
						strFlags.c_str(), name.c_str());
					return false;
				}

				flags = iter->second;
				hasBaseFlags = flags & ENTITY_BASE_DATA_FLAGS;
				if(hasBaseFlags > 0)
					scriptModule->setBase(true);

				hasCellFlags = flags & ENTITY_CELL_DATA_FLAGS;
				if(hasCellFlags > 0)
					scriptModule->setCell(true);

				hasClientFlags = flags & ENTITY_CLIENT_DATA_FLAGS;
				if(hasClientFlags > 0)
					scriptModule->setClient(true);
			}


			TiXmlNode* persistentNode = xml->enterNode(defPropertyNode->FirstChild(), "Persistent");
			if(persistentNode)
			{
				strisPersistent = xml->getValStr(persistentNode);
				std::transform(strisPersistent.begin(), strisPersistent.end(), strisPersistent.begin(), tolower);				// 转换为小写
				if(strisPersistent == "true")
					isPersistent = true;
			}

			TiXmlNode* typeNode = xml->enterNode(defPropertyNode->FirstChild(), "Type");
			if(typeNode)
			{
				strType = xml->getValStr(typeNode);
				std::transform(strType.begin(), strType.end(), strType.begin(), toupper);										// 转换为大写
				if(strType == "ARRAY")
				{
					ArrayType* dataType1 = new ArrayType();
					if(dataType1->initialize(xml, typeNode))
						dataType = dataType1;
					else
						return false;
				}
				else
					dataType = DataTypes::getDataType(strType);
			}

			TiXmlNode* identifierNode = xml->enterNode(defPropertyNode->FirstChild(), "Identifier");
			if(identifierNode)
			{
				strIdentifierNode = xml->getValStr(identifierNode);
				std::transform(strIdentifierNode.begin(), strIdentifierNode.end(), strIdentifierNode.begin(), tolower);			// 转换为小写
				if(strIdentifierNode == "true")
					isIdentifier = true;
			}

			//TiXmlNode* databaseLengthNode = xml->enterNode(defPropertyNode->FirstChild(), "Identifier");
			if(identifierNode)
			{
				databaseLength = xml->getValInt(identifierNode);
			}

			TiXmlNode* defaultValNode = xml->enterNode(defPropertyNode->FirstChild(), "Default");
			if(defaultValNode)
			{
				defaultStr = xml->getValStr(defaultValNode);
			}
			
			TiXmlNode* detailLevelNode = xml->enterNode(defPropertyNode->FirstChild(), "DetailLevel");
			if(detailLevelNode)
			{
				detailLevelStr = xml->getValStr(detailLevelNode);
				if(detailLevelStr == "FAR")
					detailLevel = DETAIL_LEVEL_FAR;
				else if(detailLevelStr == "MEDIUM")
					detailLevel = DETAIL_LEVEL_MEDIUM;
				else if(detailLevelStr == "NEAR")
					detailLevel = DETAIL_LEVEL_NEAR;
			}
			
			static ENTITY_PROPERTY_UID auto_puid = 1;
			static std::vector<ENTITY_PROPERTY_UID> puids;
			TiXmlNode* utypeValNode = xml->enterNode(defPropertyNode->FirstChild(), "Utype");
			if(utypeValNode)
			{
				futype = xml->getValInt(utypeValNode);
				puids.push_back(futype);
			}
			else
			{
				while(true)
				{
					futype = auto_puid++;
					std::vector<ENTITY_PROPERTY_UID>::iterator iter = std::find(puids.begin(), puids.end(), futype);
					if(iter == puids.end())
						break;
				}
			}

			// 产生一个属性描述实例
			PropertyDescription* propertyDescription = PropertyDescription::createDescription(futype, strType, name, flags, isPersistent, 
															dataType, isIdentifier, databaseLength, defaultStr, detailLevel);
			
			bool ret = true;
			// 添加到模块中
			if(hasCellFlags > 0)
				ret = scriptModule->addPropertyDescription(name.c_str(), propertyDescription, CELLAPP_TYPE);
			if(hasBaseFlags > 0)
				ret = scriptModule->addPropertyDescription(name.c_str(), propertyDescription, BASEAPP_TYPE);
			if(hasClientFlags > 0)
				ret = scriptModule->addPropertyDescription(name.c_str(), propertyDescription, CLIENT_TYPE);
			
			if(!ret)
			{
				ERROR_MSG("EntityDef::addPropertyDescription(%s): %s.\n", 
					moduleName.c_str(), xml->getTxdoc()->Value());
			}
		}
		XML_FOR_END(defPropertyNode);
	}
	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadDefCellMethods(std::string& moduleName, XmlPlus* xml, TiXmlNode* defMethodNode, ScriptModule* scriptModule)
{
	if(defMethodNode)
	{
		XML_FOR_BEGIN(defMethodNode)
		{
			std::string name = xml->getKey(defMethodNode);
			MethodDescription* methodDescription = new MethodDescription(0, name);
			TiXmlNode* argNode = defMethodNode->FirstChild();
			
			// 可能没有参数
			if(argNode == NULL)
				continue;

			XML_FOR_BEGIN(argNode)
			{
				std::string argType = xml->getKey(argNode);
				if(argType == "Exposed")
				{
					methodDescription->setExposed();
				}
				else if(argType == "Arg")
				{
					DataType* dataType = NULL;
					TiXmlNode* typeNode = argNode->FirstChild();
					std::string strType = xml->getValStr(typeNode);

					if(strType == "ARRAY")
					{
						ArrayType* dataType1 = new ArrayType();
						if(dataType1->initialize(xml, typeNode))
							dataType = dataType1;
					}
					else
						dataType = DataTypes::getDataType(strType);

					if(dataType == NULL)
					{
						ERROR_MSG("EntityDef::loadDefCellMethods: dataType[%s] not found, in %s!\n", strType.c_str(), name.c_str());
						return false;
					}
					methodDescription->pushArgType(dataType);
				}
				else if(argType == "Utype")
				{
					TiXmlNode* typeNode = argNode->FirstChild();
					ENTITY_METHOD_UID muid = xml->getValInt(typeNode);
					methodDescription->setUType(muid);
				}
			}
			XML_FOR_END(argNode);		
			
			// 如果配置中没有设置过utype, 则产生
			if(methodDescription->getUType() <= 0)
			{
				while(true)
				{
					ENTITY_METHOD_UID muid = g_methodUtypeAuto++;
					std::vector<ENTITY_METHOD_UID>::iterator iterutype = 
						std::find(g_methodCusUtypes.begin(), g_methodCusUtypes.end(), muid);

					if(iterutype == g_methodCusUtypes.end())
					{
						break;
					}
				}
			}

			scriptModule->addCellMethodDescription(name.c_str(), methodDescription);
		}
		XML_FOR_END(defMethodNode);
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadDefBaseMethods(std::string& moduleName, XmlPlus* xml, TiXmlNode* defMethodNode, ScriptModule* scriptModule)
{
	if(defMethodNode)
	{
		XML_FOR_BEGIN(defMethodNode)
		{
			std::string name = xml->getKey(defMethodNode);
			MethodDescription* methodDescription = new MethodDescription(0, name);
			TiXmlNode* argNode = defMethodNode->FirstChild();

			// 可能没有参数
			if(argNode == NULL)
				continue;

			XML_FOR_BEGIN(argNode)
			{
				std::string argType = xml->getKey(argNode);
				if(argType == "Exposed")
					methodDescription->setExposed();
				else if(argType == "Arg")
				{
					DataType* dataType = NULL;
					TiXmlNode* typeNode = argNode->FirstChild();
					std::string strType = xml->getValStr(typeNode);

					if(strType == "ARRAY")
					{
						ArrayType* dataType1 = new ArrayType();
						if(dataType1->initialize(xml, typeNode))
							dataType = dataType1;
					}
					else
						dataType = DataTypes::getDataType(strType);

					if(dataType == NULL)
					{
						ERROR_MSG("EntityDef::loadDefBaseMethods: dataType[%s] not found, in %s!\n", strType.c_str(), name.c_str());
						return false;
					}
					methodDescription->pushArgType(dataType);
				}
				else if(argType == "Utype")
				{
					TiXmlNode* typeNode = argNode->FirstChild();
					ENTITY_METHOD_UID muid = xml->getValInt(typeNode);
					methodDescription->setUType(muid);
				}
			}
			XML_FOR_END(argNode);		
			
			// 如果配置中没有设置过utype, 则产生
			if(methodDescription->getUType() <= 0)
			{
				while(true)
				{
					ENTITY_METHOD_UID muid = g_methodUtypeAuto++;
					std::vector<ENTITY_METHOD_UID>::iterator iterutype = 
						std::find(g_methodCusUtypes.begin(), g_methodCusUtypes.end(), muid);

					if(iterutype == g_methodCusUtypes.end())
					{
						break;
					}
				}
			}

			scriptModule->addBaseMethodDescription(name.c_str(), methodDescription);
		}
		XML_FOR_END(defMethodNode);
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadDefClientMethods(std::string& moduleName, XmlPlus* xml, TiXmlNode* defMethodNode, ScriptModule* scriptModule)
{
	if(defMethodNode)
	{
		XML_FOR_BEGIN(defMethodNode)
		{
			std::string name = xml->getKey(defMethodNode);
			MethodDescription* methodDescription = new MethodDescription(0, name);
			TiXmlNode* argNode = defMethodNode->FirstChild();

			// 可能没有参数
			if(argNode == NULL)
				continue;

			XML_FOR_BEGIN(argNode)
			{
				std::string argType = xml->getKey(argNode);
				if(argType == "Arg")
				{
					DataType* dataType = NULL;
					TiXmlNode* typeNode = argNode->FirstChild();
					std::string strType = xml->getValStr(typeNode);

					if(strType == "ARRAY")
					{
						ArrayType* dataType1 = new ArrayType();
						if(dataType1->initialize(xml, typeNode))
							dataType = dataType1;
					}
					else
						dataType = DataTypes::getDataType(strType);

					if(dataType == NULL)
					{
						ERROR_MSG("EntityDef::loadDefClientMethods: dataType[%s] not found, in %s!\n", strType.c_str(), name.c_str());
						return false;
					}
					methodDescription->pushArgType(dataType);
				}
				else if(argType == "Utype")
				{
					TiXmlNode* typeNode = argNode->FirstChild();
					ENTITY_METHOD_UID muid = xml->getValInt(typeNode);
					methodDescription->setUType(muid);
				}
			}
			XML_FOR_END(argNode);		
			
			// 如果配置中没有设置过utype, 则产生
			if(methodDescription->getUType() <= 0)
			{
				while(true)
				{
					ENTITY_METHOD_UID muid = g_methodUtypeAuto++;
					std::vector<ENTITY_METHOD_UID>::iterator iterutype = 
						std::find(g_methodCusUtypes.begin(), g_methodCusUtypes.end(), muid);

					if(iterutype == g_methodCusUtypes.end())
					{
						break;
					}
				}
			}

			scriptModule->addClientMethodDescription(name.c_str(), methodDescription);
		}
		XML_FOR_END(defMethodNode);
	}

	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::isLoadScriptModule(ScriptModule* scriptModule)
{
	switch(__loadComponentType)
	{
	case BASEAPP_TYPE:
		if(!scriptModule->hasBase())
			return false;
		break;
	case CELLAPP_TYPE:
		if(!scriptModule->hasCell())
			return false;
		break;
	case CLIENT_TYPE:
		if(!scriptModule->hasClient())
			return false;
		break;
	default:
		if(!scriptModule->hasCell())
			return false;
		break;
	};

	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::checkDefMethod(ScriptModule* scriptModule, PyObject* moduleObj, std::string& moduleName)
{
	ScriptModule::METHODDESCRIPTION_MAP* methodDescrsPtr = NULL;
	
	switch(__loadComponentType)
	{
	case BASEAPP_TYPE:
		methodDescrsPtr = (ScriptModule::METHODDESCRIPTION_MAP*)&scriptModule->getBaseMethodDescriptions();
		break;
	case CELLAPP_TYPE:
		methodDescrsPtr = (ScriptModule::METHODDESCRIPTION_MAP*)&scriptModule->getCellMethodDescriptions();
		break;
	case CLIENT_TYPE:
		methodDescrsPtr = (ScriptModule::METHODDESCRIPTION_MAP*)&scriptModule->getClientMethodDescriptions();
		break;
	default:
		methodDescrsPtr = (ScriptModule::METHODDESCRIPTION_MAP*)&scriptModule->getCellMethodDescriptions();
		break;
	};

	ScriptModule::METHODDESCRIPTION_MAP::iterator iter = methodDescrsPtr->begin();
	for(; iter != methodDescrsPtr->end(); iter++)
	{
		PyObject* pyMethod = PyObject_GetAttrString(moduleObj, const_cast<char *>(iter->first.c_str()));
		if (pyMethod != NULL)
		{
			Py_DECREF(pyMethod);
		}
		else
		{
			ERROR_MSG( "EntityDef::checkDefMethod:class %s does not have method[%s].\n",
					moduleName.c_str(), iter->first.c_str());
			return false;
		}
	}
	
	return true;	
}

//-------------------------------------------------------------------------------------
bool EntityDef::loadAllScriptModule(std::string entitiesPath, std::vector<PyTypeObject*>& scriptBaseTypes)
{
	std::string entitiesFile = entitiesPath + "entities.xml";

	SmartPointer<XmlPlus> xml(new XmlPlus());
	if(!xml.get()->openSection(entitiesFile.c_str()))
		return false;

	TiXmlNode* node = xml.get()->getRootNode();
	if(node == NULL)
		return true;

	XML_FOR_BEGIN(node)
	{
		std::string moduleName = xml.get()->getKey(node);
		ScriptModule* scriptModule = findScriptModule(moduleName.c_str());

		// 是否加载这个模块 （取决于是否在def文件中定义了与当前组件相关的方法或者属性）
		if(!isLoadScriptModule(scriptModule))
			continue;

		PyObject* pyModule = PyImport_ImportModule(const_cast<char*>(moduleName.c_str()));
		if (pyModule == NULL)
		{
			ERROR_MSG("EntityDef::initialize:Could not load module[%s]\n", moduleName.c_str());
			PyErr_Print();
			return false;
		}

		PyObject* pyClass = PyObject_GetAttrString(pyModule, const_cast<char *>(moduleName.c_str()));
		if (pyClass == NULL)
		{
			ERROR_MSG("EntityDef::initialize:Could not find class[%s]\n", moduleName.c_str());
			return false;
		}
		else 
		{
			std::string typeNames = "";
			bool valid = false;
			std::vector<PyTypeObject*>::iterator iter = scriptBaseTypes.begin();
			for(; iter != scriptBaseTypes.end(); iter++)
			{
				if(!PyObject_IsSubclass(pyClass, (PyObject *)(*iter)))
				{
					typeNames += "'";
					typeNames += (*iter)->tp_name;
					typeNames += "'";
				}
				else
				{
					valid = true;
					break;
				}
			}
			
			if(!valid)
			{
				ERROR_MSG("EntityDef::initialize:Class %s is not derived from KBEngine.[%s]\n", moduleName.c_str(), typeNames.c_str());
				return false;
			}
		}

		if(!PyType_Check(pyClass))
		{
			ERROR_MSG("EntityDef::initialize:class[%s] is valid!\n", moduleName.c_str());
			return false;
		}
		
		if(!checkDefMethod(scriptModule, pyClass, moduleName))
		{
			ERROR_MSG("EntityDef::initialize:class[%s] checkDefMethod is failed!\n", moduleName.c_str());
			return false;
		}
		
		DEBUG_MSG("loaded script:%s(%u).\n", moduleName.c_str(), scriptModule->getUType());
		scriptModule->setScriptType((PyTypeObject *)pyClass);
		S_RELEASE(pyModule);
	}
	XML_FOR_END(node);

	return true;
}

//-------------------------------------------------------------------------------------
ScriptModule* EntityDef::findScriptModule(ENTITY_SCRIPT_UID utype)
{
	if (utype >= __scriptModules.size())
	{
		ERROR_MSG("EntityDef::findScriptModule: is not exist(utype:%d)!\n", utype);
		return NULL;
	}

	return __scriptModules[utype];
}

//-------------------------------------------------------------------------------------
ScriptModule* EntityDef::findScriptModule(const char* scriptName)
{
	std::map<std::string, ENTITY_SCRIPT_UID>::iterator iter = __scriptTypeMappingUType.find(scriptName);
	if(iter == __scriptTypeMappingUType.end())
	{
		ERROR_MSG("EntityDef::findScriptModule: [%s] not found!\n", scriptName);
		return NULL;
	}

	return findScriptModule(iter->second);
}

//-------------------------------------------------------------------------------------
bool EntityDef::installScript(PyObject* mod)
{
	EntityMailbox::installScript(mod);
	return true;
}

//-------------------------------------------------------------------------------------
bool EntityDef::uninstallScript()
{
	EntityMailbox::uninstallScript();
	return true;
}

//-------------------------------------------------------------------------------------
}
