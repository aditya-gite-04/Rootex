#include "resource_file.h"

#include <bitset>
#include <sstream>

#include "resource_loader.h"
#include "framework/systems/audio_system.h"
#include "renderer/rendering_device.h"
#include "interpreter.h"

ResourceFile::ResourceFile(const Type& type, ResourceData* resData)
    : m_Type(type)
    , m_ResourceData(resData)
{
	PANIC(resData == nullptr, "Null resource found. Resource of this type has not been loaded correctly: " + std::to_string((int)type));
	m_LastReadTime = OS::s_FileSystemClock.now();
	m_LastChangedTime = OS::GetFileLastChangedTime(getPath().string());
}

void ResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<ResourceFile> resourceFile = rootex.new_usertype<ResourceFile>("ResourceFile");
	resourceFile["isValid"] = &ResourceFile::isValid;
	resourceFile["isDirty"] = &ResourceFile::isDirty;
	resourceFile["isOpen"] = &ResourceFile::isOpen;
	resourceFile["getPath"] = [](ResourceFile& f) { return f.getPath().string(); };
	resourceFile["getType"] = &ResourceFile::getType;
}

ResourceFile::~ResourceFile()
{
}

bool ResourceFile::isValid()
{
	return m_ResourceData != nullptr;
}

bool ResourceFile::isOpen()
{
	return m_ResourceData == nullptr;
}

FilePath ResourceFile::getPath() const
{
	return m_ResourceData->getPath();
}

ResourceFile::Type ResourceFile::getType() const
{
	return m_Type;
}

ResourceData* ResourceFile::getData()
{
	return m_ResourceData;
}

const FileTimePoint& ResourceFile::getLastChangedTime()
{
	m_LastChangedTime = OS::GetFileLastChangedTime(getPath().string());
	return m_LastChangedTime;
}

bool ResourceFile::isDirty()
{
	return getLastReadTime() < getLastChangedTime();
}

AudioResourceFile::AudioResourceFile(ResourceData* resData)
    : ResourceFile(Type::Audio, resData)
    , m_AudioDataSize(0)
    , m_BitDepth(0)
    , m_Channels(0)
    , m_DecompressedAudioBuffer(nullptr)
    , m_Format(0)
    , m_Frequency(0)
{
}

AudioResourceFile::~AudioResourceFile()
{
}

void AudioResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<AudioResourceFile> audioResourceFile = rootex.new_usertype<AudioResourceFile>(
	    "AudioResourceFile",
	    sol::base_classes, sol::bases<ResourceFile>());
	audioResourceFile["getAudioDataSize"] = &AudioResourceFile::getAudioDataSize;
	audioResourceFile["getFormat"] = &AudioResourceFile::getFormat;
	audioResourceFile["getFrequency"] = &AudioResourceFile::getFrequency;
	audioResourceFile["getBitDepth"] = &AudioResourceFile::getBitDepth;
	audioResourceFile["getChannels"] = &AudioResourceFile::getChannels;
	audioResourceFile["getDuration"] = &AudioResourceFile::getDuration;
}

TextResourceFile::TextResourceFile(const Type& type, ResourceData* resData)
    : ResourceFile(type, resData)
{
}

TextResourceFile::~TextResourceFile()
{
}

void TextResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<TextResourceFile> textResourceFile = rootex.new_usertype<TextResourceFile>(
	    "TextResourceFile",
	    sol::base_classes, sol::bases<ResourceFile>());
	textResourceFile["getString"] = &TextResourceFile::getString;
}

void TextResourceFile::putString(const String& newData)
{
	*m_ResourceData->getRawData() = FileBuffer(newData.begin(), newData.end());
}

void TextResourceFile::popBack()
{
	m_ResourceData->getRawData()->pop_back();
}

void TextResourceFile::append(const String& add)
{
	m_ResourceData->getRawData()->insert(m_ResourceData->getRawData()->end(), add.begin(), add.end());
}

String TextResourceFile::getString() const
{
	return String(
	    m_ResourceData->getRawData()->begin(),
	    m_ResourceData->getRawData()->end());
}

LuaTextResourceFile::LuaTextResourceFile(ResourceData* resData)
    : TextResourceFile(Type::Lua, resData)
{
}

LuaTextResourceFile::~LuaTextResourceFile()
{
}

void LuaTextResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<LuaTextResourceFile> luaTextResourceFile = rootex.new_usertype<LuaTextResourceFile>(
	    "LuaTextResourceFile",
	    sol::base_classes, sol::bases<ResourceFile, TextResourceFile>());
}

ModelResourceFile::ModelResourceFile(ResourceData* resData)
    : ResourceFile(Type::Model, resData)
{
}

ModelResourceFile::~ModelResourceFile()
{
}

void ModelResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<ModelResourceFile> modelResourceFile = rootex.new_usertype<ModelResourceFile>(
	    "ModelResourceFile",
	    sol::base_classes, sol::bases<ResourceFile>());
}

AnimatedModelResourceFile::AnimatedModelResourceFile(ResourceData* resData)
    : ResourceFile(Type::AnimatedModel, resData)
{
}

AnimatedModelResourceFile::~AnimatedModelResourceFile()
{
}

void AnimatedModelResourceFile::RegisterAPI(sol::state& rootex)
{
	sol::usertype<AnimatedModelResourceFile> animatedModelResourceFile = rootex.new_usertype<AnimatedModelResourceFile>(
	    "AnimatedModelResourceFile",
	    sol::base_classes, sol::bases<ResourceFile>());
}

Matrix AnimatedModelResourceFile::AiMatrixToMatrix(const aiMatrix4x4& aiMatrix)
{
	return Matrix(
		aiMatrix.a1, aiMatrix.a2, aiMatrix.a3, aiMatrix.a4,
		aiMatrix.b1, aiMatrix.b2, aiMatrix.b3, aiMatrix.b4,
		aiMatrix.c1, aiMatrix.c2, aiMatrix.c3, aiMatrix.c4,
		aiMatrix.d1, aiMatrix.d2, aiMatrix.d3, aiMatrix.d4
	).Transpose();
}

Vector<String> AnimatedModelResourceFile::getAnimationNames()
{
	Vector<String> animationNames;
	for (auto& element : m_Animations)
	{
		animationNames.push_back(element.first);
	}
	return animationNames;
}

float AnimatedModelResourceFile::getAnimationEndTime(const String& animationName)
{
	return m_Animations[animationName].getEndTime();
}

void AnimatedModelResourceFile::setRootTransform(aiNode* currentNode, Matrix parentToRootTransform, bool& isFound)
{
	Matrix toParentTransformation = AiMatrixToMatrix(currentNode->mTransformation);
	Matrix currentToRootTransform = toParentTransformation * parentToRootTransform;
	if (!isFound)
	{
		if (m_BoneMapping.find(currentNode->mName.C_Str()) != m_BoneMapping.end())
		{
			m_RootInverseTransform = currentToRootTransform.Invert();
			isFound = true;
		}
		else
		{
			for (unsigned int i = 0; i < currentNode->mNumChildren; i++)
			{
				setRootTransform(currentNode->mChildren[i], currentToRootTransform, isFound);
			}
		}
	}
}

void AnimatedModelResourceFile::setNodeHeirarchy(aiNode* currentAiNode, SkeletonNode* currentNode)
{
	static unsigned int nodeCount = 0;
	currentNode->m_Index = nodeCount;
	currentNode->m_Name = String(currentAiNode->mName.C_Str());
	currentNode->m_LocalBindTransform = AiMatrixToMatrix(currentAiNode->mTransformation);

	currentNode->m_Children.resize(currentAiNode->mNumChildren);
	for (int i = 0; i < currentAiNode->mNumChildren; i++)
	{
		nodeCount++;
		currentNode->m_Children[i] = new SkeletonNode;
		setNodeHeirarchy(currentAiNode->mChildren[i], currentNode->m_Children[i]);
	}
}

void AnimatedModelResourceFile::getFinalTransforms(const String& animationName, float currentTime, Vector<Matrix>& transforms)
{
	getAnimationTransforms(m_RootNode, currentTime, animationName, Matrix::Identity);

	for (unsigned int i = 0; i < getBoneCount(); i++)
	{
		transforms[i] = m_BoneOffsets[i] * m_LocalAnimationTransforms[i] * m_RootInverseTransform;
	}
}

void AnimatedModelResourceFile::getAnimationTransforms(SkeletonNode* node, float currentTime, const String& animationName, const Matrix& parentModelTransform)
{
	Matrix boneSpaceTransform = m_Animations[animationName].interpolate(node->m_Name.c_str(), currentTime);
	if (boneSpaceTransform == Matrix::Identity)
	{
		boneSpaceTransform = node->m_LocalBindTransform;
	}
	
	Matrix currentModelTransform = boneSpaceTransform * parentModelTransform;

	if (m_BoneMapping.find(node->m_Name.c_str()) != m_BoneMapping.end())
	{
		m_LocalAnimationTransforms[m_BoneMapping[node->m_Name]] = currentModelTransform;
	}

	for (auto& child : node->m_Children)
	{
		getAnimationTransforms(child, currentTime, animationName, currentModelTransform);
	}
}

void AnimatedModelResourceFile::setInverseBindTransforms(aiNode* currentNode, const Matrix& parentModelTransform)
{
	Matrix localBindTransform = AiMatrixToMatrix(currentNode->mTransformation);
	Matrix currentModelTransform = localBindTransform * parentModelTransform;

	if (m_BoneMapping.find(currentNode->mName.C_Str()) != m_BoneMapping.end())
	{
		unsigned int index = m_BoneMapping.at(currentNode->mName.C_Str());
		m_InverseBindTransforms[index] = currentModelTransform.Invert();
	}

	for (unsigned int i = 0; i < currentNode->mNumChildren; i++)
	{
		setInverseBindTransforms(currentNode->mChildren[i], currentModelTransform);
	}
}

ImageResourceFile::ImageResourceFile(ResourceData* resData)
    : ResourceFile(Type::Image, resData)
{
}

ImageResourceFile::~ImageResourceFile()
{
}

void ImageResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<ImageResourceFile> imageResourceFile = rootex.new_usertype<ImageResourceFile>(
	    "ImageResourceFile",
	    sol::base_classes, sol::bases<ResourceFile>());
}

FontResourceFile::FontResourceFile(ResourceData* resData)
    : ResourceFile(Type::Font, resData)
{
	regenerateFont();
}

FontResourceFile::~FontResourceFile()
{
}

void FontResourceFile::regenerateFont()
{
	m_Font = RenderingDevice::GetSingleton()->createFont(m_ResourceData->getRawData());
	m_Font->SetDefaultCharacter('X');
}

void FontResourceFile::RegisterAPI(sol::table& rootex)
{
	sol::usertype<FontResourceFile> fontResourceFile = rootex.new_usertype<FontResourceFile>(
	    "FontResourceFile",
	    sol::base_classes, sol::bases<ResourceFile>());
}
