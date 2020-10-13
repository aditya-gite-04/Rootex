#include "material_library.h"

#include "core/resource_loader.h"

MaterialLibrary::MaterialMap MaterialLibrary::s_Materials;
const String MaterialLibrary::s_DefaultMaterialPath = "rootex/assets/materials/default.rmat";
const String MaterialLibrary::s_AnimatedDefaultMaterialPath = "rootex/assets/materials/animated_default.rmat";

MaterialLibrary::MaterialDatabase MaterialLibrary::s_MaterialDatabase = {
	{ BasicMaterial::s_MaterialName, { BasicMaterial::CreateDefault, BasicMaterial::Create } },
	{ SkyMaterial::s_MaterialName, { SkyMaterial::CreateDefault, SkyMaterial::Create } },
	{ AnimatedMaterial::s_MaterialName, { AnimatedMaterial::CreateDefault, AnimatedMaterial::Create } }
};

void MaterialLibrary::PopulateMaterials(const String& path)
{
	for (auto& materialFile : OS::GetAllInDirectory(path))
	{
		if (OS::IsFile(materialFile.generic_string()) && materialFile.extension() == ".rmat")
		{
			TextResourceFile* materialResourceFile = ResourceLoader::CreateTextResourceFile(materialFile.generic_string());
			const JSON::json& materialJSON = JSON::json::parse(materialResourceFile->getString());
			s_Materials[materialFile.generic_string()] = { (String)materialJSON["type"], {} };
		}
	}
}

bool MaterialLibrary::IsDefault(const String& materialPath)
{
	static String rootex = "rootex";
	return materialPath.substr(0, rootex.size()) == rootex;
}

void MaterialLibrary::LoadMaterials()
{
	PopulateMaterials("game/assets/");
	PopulateMaterials("rootex/assets/");
}

Ref<Material> MaterialLibrary::GetMaterial(const String& materialPath)
{
	if (s_Materials.find(materialPath) == s_Materials.end())
	{
		WARN("Material file not found, returning default material instead of: " + materialPath);
		return GetDefaultMaterial();
	}
	else
	{
		if (Ref<Material> lockedMaterial = s_Materials[materialPath].second.lock())
		{
			return lockedMaterial;
		}
		else
		{
			TextResourceFile* materialFile = ResourceLoader::CreateTextResourceFile(materialPath);
			const JSON::json materialJSON = JSON::json::parse(materialFile->getString());
			Ref<Material> material(s_MaterialDatabase[materialJSON["type"]].second(materialJSON));
			material->setFileName(materialPath);
			s_Materials[materialPath].second = material;
			return material;
		}
	}
}

Ref<Material> MaterialLibrary::GetDefaultMaterial()
{
	if (Ref<Material> lockedMaterial = s_Materials[s_DefaultMaterialPath].second.lock())
	{
		return lockedMaterial;
	}
	else
	{
		Ref<Material> material(BasicMaterial::CreateDefault());
		material->setFileName(s_DefaultMaterialPath);
		s_Materials[s_DefaultMaterialPath].second = material;
		return material;
	}
}

Ref<Material> MaterialLibrary::GetAnimatedDefaultMaterial()
{
	if (Ref<Material> lockedMaterial = s_Materials[s_AnimatedDefaultMaterialPath].second.lock())
	{
		return lockedMaterial;
	}
	else
	{
		Ref<Material> material(AnimatedMaterial::CreateDefault());
		material->setFileName(s_AnimatedDefaultMaterialPath);
		s_Materials[s_AnimatedDefaultMaterialPath].second = material;
		return material;
	}
}

void MaterialLibrary::SaveAll()
{
	for (auto& [materialPath, materialInfo] : s_Materials)
	{
		if (IsDefault(materialPath))
		{
			continue;
		}
		if (Ref<Material> lockedMaterial = materialInfo.second.lock())
		{
			TextResourceFile* materialFile = ResourceLoader::CreateNewTextResourceFile(materialPath);
			materialFile->putString(lockedMaterial->getJSON().dump(4));
			ResourceLoader::SaveResourceFile(materialFile);
		}
	}
}

void MaterialLibrary::CreateNewMaterialFile(const String& materialPath, const String& materialType)
{
	if (materialPath == s_DefaultMaterialPath)
	{
		return;
	}

	if (s_Materials.find(materialPath) == s_Materials.end())
	{
		if (!OS::IsExists(materialPath))
		{
			TextResourceFile* materialFile = ResourceLoader::CreateNewTextResourceFile(materialPath);
			Ref<Material> material(s_MaterialDatabase[materialType].first());
			materialFile->putString(material->getJSON().dump(4));
			ResourceLoader::SaveResourceFile(materialFile);
			s_Materials[materialPath] = { materialType, {} };
			PRINT("Created material: " + materialPath + " of type " + materialType);
			
			return;
		}
	}
}

bool MaterialLibrary::IsExists(const String& materialPath)
{
	return OS::IsExists(materialPath);
}
