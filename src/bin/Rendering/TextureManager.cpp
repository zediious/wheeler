#include "TextureManager.h"
#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "include/lib/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "include/lib/nanosvgrast.h"

#include <d3d11.h>


void Texture::Init()
{
	//load_images(image_type_name_map, image_struct, img_directory);
	load_images(icon_type_name_map, icon_struct, icon_directory);
	load_custom_icon_images();
	//load_images(key_icon_name_map, key_struct, key_directory);
	//load_images(default_key_icon_name_map, default_key_struct, key_directory);
	//load_images(gamepad_ps_icon_name_map, ps_key_struct, key_directory);
	//load_images(gamepad_xbox_icon_name_map, xbox_key_struct, key_directory);
}
Texture::Image Texture::GetIconImage(icon_image_type a_imageType, RE::TESForm* a_form)
{
	// look for formId matches
	if (a_form) {
		if (icon_struct_formID.contains(a_form->GetFormID())) {
			return icon_struct_formID[a_form->GetFormID()];
		}
		// look for keyword matches
		const auto keywordForm = a_form->As<RE::BGSKeywordForm>();
		if (keywordForm) {
			for (auto& entry : icon_struct_keyword) {
				if (keywordForm->HasKeywordString(entry.first)) {
					return entry.second;
				}
			}
		}
	}
	// look for type matches
	return icon_struct[static_cast<int32_t>(a_imageType)];
}

bool Texture::load_texture_from_file(const char* filename, REX::W32::ID3D11ShaderResourceView** out_srv, int& out_width, int& out_height)
{
	IM_ASSERT(device_ != nullptr);
	auto* render_manager = RE::BSGraphics::Renderer::GetSingleton();
	if (!render_manager) {
		logger::error("Cannot find render manager. Initialization failed."sv);
		return false;
	}

	REX::W32::ID3D11Device* forwarder = render_manager->GetRendererData()->forwarder;

	// Load from disk into a raw RGBA buffer
	auto* svg = nsvgParseFromFile(filename, "px", 96.0f);
	auto* rast = nsvgCreateRasterizer();

	auto image_width = static_cast<int>(svg->width);
	auto image_height = static_cast<int>(svg->height);

	auto image_data = (unsigned char*)malloc(image_width * image_height * 4);
	nsvgRasterize(rast, svg, 0, 0, 1, image_data, image_width, image_height, image_width * 4);
	nsvgDelete(svg);
	nsvgDeleteRasterizer(rast);

	// Create texture
	REX::W32::D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.width = image_width;
	desc.height = image_height;
	desc.mipLevels = 1;
	desc.arraySize = 1;
	desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.sampleDesc.count = 1;
	desc.usage = REX::W32::D3D11_USAGE_DEFAULT;
	desc.bindFlags = REX::W32::D3D11_BIND_SHADER_RESOURCE;
	desc.cpuAccessFlags = 0;
	desc.miscFlags = 0;

	REX::W32::ID3D11Texture2D* p_texture = nullptr;
	REX::W32::D3D11_SUBRESOURCE_DATA sub_resource;
	sub_resource.sysMem = image_data;
	sub_resource.sysMemPitch = desc.width * 4;
	sub_resource.sysMemSlicePitch = 0;
	forwarder->CreateTexture2D(&desc, &sub_resource, &p_texture);

	// Create texture view
	REX::W32::D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	ZeroMemory(&srv_desc, sizeof srv_desc);
	srv_desc.format = REX::W32::DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.viewDimension = REX::W32::D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.texture2D.mipLevels = desc.mipLevels;
	srv_desc.texture2D.mostDetailedMip = 0;
	forwarder->CreateShaderResourceView(p_texture, &srv_desc, out_srv);
	p_texture->Release();

	free(image_data);

	out_width = image_width;
	out_height = image_height;

	return true;
}

static int hexStringToInt(const std::string& hexString)
{
	// Remove the "0x" prefix if it exists
	std::string hexValue = hexString;
	if (hexString.substr(0, 2) == "0x") {
		hexValue = hexString.substr(2);
	}

	// Convert the hex string to an integer
	std::stringstream ss;
	ss << std::hex << hexValue;
	int intValue;
	ss >> intValue;

	return intValue;
}

void Texture::load_custom_icon_images()
{
	auto tesDataHandler = RE::TESDataHandler::GetSingleton();
	if (!tesDataHandler) {
		return;
	}
	for (const auto& entry : std::filesystem::directory_iterator(icon_custom_directory)) {
		if (entry.path().filename().extension() != ".svg") {
			continue;
		}
		Image img;
		std::string fileName = entry.path().filename().string();
		bool fid = false;  // whether we're looking for a formID match
		size_t idx = fileName.find("FID_");
		if (idx == 0) {
			fid = true;
		} else {
			idx = fileName.find("KWD_");
			if (idx != 0) {
				continue;  // not a valid file name
			}
		}
		if (fid) {
			const size_t pluginNameBegin = 4;
			size_t pluginNameEnd = fileName.find("_0x", pluginNameBegin);  // find the 2nd '_'
			if (pluginNameEnd == std::string::npos) {
				pluginNameEnd = fileName.find("_0X", pluginNameBegin);
				if (pluginNameEnd == std::string::npos) {
					continue;
				}
			}
			std::string pluginName = fileName.substr(pluginNameBegin, pluginNameEnd - pluginNameBegin);
			size_t formIdEnd = fileName.find(".svg");
			size_t formIdBegin = pluginNameEnd + 1;
			std::string formIdStr = fileName.substr(formIdBegin, formIdEnd - formIdBegin);
			int formId = hexStringToInt(formIdStr);
			RE::TESForm* form = tesDataHandler->LookupForm((uint32_t)formId, pluginName);
			if (!form) {
				continue;
			}
			RE::FormID formID = form->GetFormID();
			Image img;
			load_texture_from_file(entry.path().string().c_str(), &img.texture, img.width, img.height);
			icon_struct_formID[formID] = img;
		} else { //kwd
			const size_t keywordBegin = 4;
			size_t keywordEnd = fileName.find(".svg");
			std::string keyWord = fileName.substr(keywordBegin, keywordEnd - keywordBegin);
			Image img;
			load_texture_from_file(entry.path().string().c_str(), &img.texture, img.width, img.height);
			icon_struct_keyword[keyWord] = img;
		}
	}
}
