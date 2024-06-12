/*
 * Copyright 2016 Nu-book Inc.
 * Copyright 2023 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#include "ReadBarcode.h" // 引入头文件

#include <emscripten/bind.h> // 引入Emscripten绑定库
#include <emscripten/val.h> // 引入Emscripten值类型库
#include <memory> // 引入智能指针库
#include <stdexcept> // 引入异常处理库
#include <string> // 引入字符串库

#define STB_IMAGE_IMPLEMENTATION // 定义STB图像实现宏
#include <stb_image.h> // 引入STB图像库

using namespace ZXing; // 使用ZXing命名空间

// 定义读取结果结构体
struct ReadResult
{
	std::string format{}; // 格式
	std::string text{}; // 文本
	emscripten::val bytes; // 字节数据
	std::string error{}; // 错误信息
	Position position{}; // 位置信息
	std::string symbologyIdentifier{}; // 符号标识符
};

// 读取条形码函数
std::vector<ReadResult> readBarcodes(ImageView iv, bool tryHarder, const std::string& format, int maxSymbols)
{
	try {
		ReaderOptions opts; // 创建阅读器选项对象
		opts.setTryHarder(tryHarder); // 设置尝试更努力解码
		opts.setTryRotate(tryHarder); // 设置尝试旋转解码
		opts.setTryInvert(tryHarder); // 设置尝试反转解码
		opts.setTryDownscale(tryHarder); // 设置尝试缩小解码
		opts.setFormats(BarcodeFormatsFromString(format)); // 设置格式
		opts.setMaxNumberOfSymbols(maxSymbols); // 设置最大符号数量
//		opts.setReturnErrors(maxSymbols > 1);

		auto barcodes = ReadBarcodes(iv, opts); // 读取条形码

		std::vector<ReadResult> readResults{}; // 创建读取结果向量
		readResults.reserve(barcodes.size()); // 预留空间

		thread_local const emscripten::val Uint8Array = emscripten::val::global("Uint8Array"); // 获取Uint8Array全局对象

		for (auto&& barcode : barcodes) { // 遍历条形码
			const ByteArray& bytes = barcode.bytes(); // 获取字节数据
			readResults.push_back({ // 将读取结果添加到向量中
				ToString(barcode.format()), // 格式
				barcode.text(), // 文本
				Uint8Array.new_(emscripten::typed_memory_view(bytes.size(), bytes.data())), // 字节数据
				ToString(barcode.error()), // 错误信息
				barcode.position(), // 位置信息
				barcode.symbologyIdentifier() // 符号标识符
			});
		}

		return readResults; // 返回读取结果向量
	} catch (const std::exception& e) { // 捕获异常
		return {{"", "", {}, e.what()}}; // 返回异常信息
	} catch (...) { // 捕获未知异常
		return {{"", "", {}, "Unknown error"}}; // 返回未知错误信息
	}
	return {}; // 返回空向量
}

// 从图像读取条形码函数
std::vector<ReadResult> readBarcodesFromImage(int bufferPtr, int bufferLength, bool tryHarder, std::string format, int maxSymbols)
{
	int width, height, channels; // 定义宽度、高度和通道变量
	std::unique_ptr<stbi_uc, void (*)(void*)> buffer(
		stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bufferPtr), bufferLength, &width, &height, &channels, 1), // 从内存加载图像
		stbi_image_free); // 释放图像内存
	if (buffer == nullptr) // 如果缓冲区为空
		return {{"", "", {}, "Error loading image"}}; // 返回错误信息

// 调用读取条形码函数并返回结果
	return readBarcodes({buffer.get(), width, height, ImageFormat::Lum}, tryHarder, format, maxSymbols)
}

// 从图像读取单个条形码函数
ReadResult readBarcodeFromImage(int bufferPtr, int bufferLength, bool tryHarder, std::string format)
{
    // 调用从图像读取条形码函数并返回第一个结果或默认结果
	return FirstOrDefault(readBarcodesFromImage(bufferPtr, bufferLength, tryHarder, format, 1));
}

// 从像素图读取条形码函数
std::vector<ReadResult> readBarcodesFromPixmap(int bufferPtr, int imgWidth, int imgHeight, bool tryHarder, std::string format, int maxSymbols
{
	// 调用读取条形码函数并返回结果
	return readBarcodes({reinterpret_cast<uint8_t*>(bufferPtr), imgWidth, imgHeight, ImageFormat::RGBA}, tryHarder, format, maxSymbols);
}

// 从像素图读取单个条形码函数
ReadResult readBarcodeFromPixmap(int bufferPtr, int imgWidth, int imgHeight, bool tryHarder, std::string format
{
    // 调用从像素图读取条形码函数并返回第一个结果或默认结果
	return FirstOrDefault(readBarcodesFromPixmap(bufferPtr, imgWidth, imgHeight, tryHarder, format, 1));
}

// Emscripten绑定函数
EMSCRIPTEN_BINDINGS(BarcodeReader)
{
	using namespace emscripten; // 使用Emscripten命名空间

	value_object<ReadResult>("ReadResult") // 定义读取结果对象
		.field("format", &ReadResult::format) // 格式字段
		.field("text", &ReadResult::text) // 文本字段
		.field("bytes", &ReadResult::bytes) // 字节数据字段
		.field("error", &ReadResult::error) // 错误信息字段
		.field("position", &ReadResult::position) // 位置信息字段
		.field("symbologyIdentifier", &ReadResult::symbologyIdentifier); // 符号标识符字段

	value_object<ZXing::PointI>("Point").field("x", &ZXing::PointI::x).field("y", &ZXing::PointI::y); // 定义点对象

	value_object<ZXing::Position>("Position") // 定义位置对象
		.field("topLeft", emscripten::index<0>()) // 左上角字段
		.field("topRight", emscripten::index<1>()) // 右上角字段
		.field("bottomRight", emscripten::index<2>()) // 右下角字段
		.field("bottomLeft", emscripten::index<3>()); // 左下角字段

	register_vector<ReadResult>("vector<ReadResult>"); // 注册读取结果向量类型

	function("readBarcodeFromImage", &readBarcodeFromImage); // 注册从图像读取单个条形码函数
	function("readBarcodeFromPixmap", &readBarcodeFromPixmap); // 注册从像素图读取单个条形码函数

	function("readBarcodesFromImage", &readBarcodesFromImage); // 注册从图像读取条形码函数
	function("readBarcodesFromPixmap", &readBarcodesFromPixmap); // 注册从像素图读取条形码函数
};
