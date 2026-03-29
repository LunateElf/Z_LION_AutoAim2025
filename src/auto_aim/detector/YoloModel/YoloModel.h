#ifndef _YOLOMODEL_H_
#define _YOLOMODEL_H_
#include"../Light/Light.h"
#include"../Armor/Armor.h"
#include <opencv2/opencv.hpp>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>
#include <iostream>
#include <memory>
#include <string>
namespace rm
{
	struct TrtLogger : public nvinfer1::ILogger {
		void log(Severity severity, const char* msg) noexcept override {
			if (severity <= Severity::kWARNING) {
				std::cerr << "[TensorRT] " << msg << std::endl;
			}
		}
	};

	template <typename T>
	struct TrtDeleter {
		void operator()(T* obj) const {
			if (obj) {
				#if defined(NV_TENSORRT_MAJOR) && (NV_TENSORRT_MAJOR >= 10)
				delete obj;
				#else
				obj->destroy();
				#endif
			}
		}
	};

	class YoloModel
	{
	protected:
		// 0  3
		// 1  2
		// ����,����,����,����
		struct alignas(4) bbox_t {
			cv::Point2f pts[4];
			float confidence;
			int label;// 
		};
	public:
		YoloModel() {};
		YoloModel(std::string model_path, int image_size);
		void set_enemy_color(bool enemy_blue);
		virtual ~YoloModel();
	public:
		// Ѱ��װ�װ�
		virtual std::vector<Armor> find_armors(cv::Mat src);

	protected:
		// �������౩¶bbox_t,���û�͸��
		std::vector<bbox_t> forward(cv::Mat src);
		// Ŀ�곬����
		std::vector<bbox_t> screen_out_edge_targets(const std::vector<bbox_t>& bbox_ts,
			double image_width, double image_height);
		// ����Ӧ�ߴ�
		cv::Mat letterbox(cv::Mat& src, int w, int h, int& padd_w_, int& padd_h_);
		cv::Mat letterbox(cv::Mat& src, int w, int h);
		// ȡ��ֵ��ǩ
		int argmax(const float* ptr, int len);
		// sigmoid����
		float sigmoid(float x);
		// ��Сֵ
		float min_4(const float pts1[4]);
		// ���ֵ
		float max_4(const float pts1[4]);
		// �Ƿ񳬳��غ�����
		bool is_overlap(const float pts1[4], const float pts2[4]);
	private:
		// ��ǩ����,�����ɫ����
		bool strip_filter(int label,int enemy_blue);
	private:
		int image_size; // ���÷����ߴ紦����ͼ���С����
		// ��չ��ͼƬ����
		int padd_w_ = 0;
		int padd_h_ = 0;
		bool enemy_blue; // �з���ɫ
	private:
		TrtLogger logger_;
		std::unique_ptr<nvinfer1::IRuntime, TrtDeleter<nvinfer1::IRuntime>> runtime_;
		std::unique_ptr<nvinfer1::ICudaEngine, TrtDeleter<nvinfer1::ICudaEngine>> engine_;
		std::unique_ptr<nvinfer1::IExecutionContext, TrtDeleter<nvinfer1::IExecutionContext>> context_;
		int input_index_ = -1;
		int output_index_ = -1;
		std::string input_tensor_name_;
		std::string output_tensor_name_;
		size_t input_buffer_size_ = 0;
		size_t output_buffer_size_ = 0;
		void* device_buffers_[2] = { nullptr, nullptr };
		cudaStream_t stream_ = nullptr;
	};

};
#endif // !YOLOMODEL_H
