#pragma once


namespace Lumos
{
	namespace Graphics
	{
		class TextureCube;
		class Texture;

		class Environment
		{
		public:
			Environment();
			Environment(TextureCube* env)
			{
				m_Environmnet = Ref<TextureCube>(env);
				m_PrefilteredEnvironment = nullptr;
				m_IrradianceMap = nullptr;
			}

			Environment(TextureCube* env, TextureCube* irr)
			{
				m_Environmnet = Ref<TextureCube>(env);
				m_IrradianceMap = Ref<TextureCube>(irr);
				m_PrefilteredEnvironment = nullptr;
			}

			Environment(const std::string& filepath, bool genPrefilter, bool genIrradiance);
			Environment(const std::string& name, u32 numMip, u32 width, u32 height, const std::string& fileType = ".tga");
			~Environment();

			void Load(const std::string& name, u32 numMip, u32 width, u32 height, const std::string& fileType = ".tga");
			void Load();

			TextureCube* GetEnvironmentMap() const
			{
				return m_Environmnet.get();
			}
			TextureCube* GetPrefilteredMap() const
			{
				return m_PrefilteredEnvironment.get();
			}
			TextureCube* GetIrradianceMap() const
			{
				return m_IrradianceMap.get();
			}

			void SetEnvironmnet(TextureCube* environmnet);
			void SetPrefilteredEnvironment(TextureCube* prefilteredEnvironment);
			void SetIrradianceMap(TextureCube* irradianceMap);

			template<class Archive>
			void save(Archive& archive) const
			{
				archive(m_FilePath, m_NumMips, m_Width, m_Height, m_FileType);
			}

			template<class Archive>
			void load(Archive& archive)
			{
				archive(m_FilePath, m_NumMips, m_Width, m_Height, m_FileType);
				if(m_FilePath != "")
					Load();
			}
			
			const std::string& GetFilePath() const { return m_FilePath; } 
			const std::string& GetFileType() const { return m_FileType; }
			u32 GetNumMips() { return m_NumMips; }
			u32 GetWidth() { return m_Width; }
			u32 GetHeight() { return m_Height; }
			
			void SetFilePath(const std::string& path) { m_FilePath = path; } 
			void SetFileType(const std::string& type) { m_FileType = type; }
			 void SetNumMips(u32 num) { m_NumMips = num; }
			void SetWidth(u32 width) { m_Width = width; }
			void SetHeight(u32 height) { m_Height = height; }
			
		private:
			Ref<TextureCube> m_Environmnet;
			Ref<TextureCube> m_PrefilteredEnvironment;
			Ref<TextureCube> m_IrradianceMap;
			
			u32 m_NumMips = 0;
			u32 m_Width = 0;
			u32 m_Height = 0;
			std::string m_FilePath;
			std::string m_FileType;
		};
	}
}