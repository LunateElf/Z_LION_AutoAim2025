#include"YoloModel.h"
#include <numeric>
#include <stdexcept>

namespace rm
{

    int YoloModel::argmax(const float* ptr, int len) {
        int max_arg = 0;
        for (int i = 1; i < len; i++) {
            if (ptr[i] > ptr[max_arg]) max_arg = i;
        }
        return max_arg;
    };

    float YoloModel::sigmoid(float x) {
        return 1 / (1 + std::exp(-x));
    }

    float YoloModel::min_4(const float pts1[4]) {
        float min = pts1[0];
        for (int i = 1; i < 4; i++) {
            if (min > pts1[i])
                min = pts1[i];
        };
        return min;
    };

    float YoloModel::max_4(const float pts1[4]) {
        float max = pts1[0];
        for (int i = 1; i < 4; i++) {
            if (max < pts1[i])
                max = pts1[i];
        };
        return max;
    };

    bool YoloModel::is_overlap(const float pts1[4], const float pts2[4]) {
        cv::Rect2f bbox1, bbox2;
        bbox1.x = pts1[0];
        bbox1.y = pts1[1];
        bbox1.width = pts1[2];
        bbox1.height = pts1[3];
        bbox2.x = pts2[0];
        bbox2.y = pts2[1];
        bbox2.width = pts2[2];
        bbox2.height = pts2[3];
        return (bbox1 & bbox2).area() > 0;
    }
    bool YoloModel::strip_filter(int label, int enemy_blue)
    {
        if (label >= 10) return false;
        if (enemy_blue) {
            return label < 5;
        }
        else {
            return label >= 5 && label < 10;
        };
    }
    ;

    cv::Mat YoloModel::letterbox(cv::Mat& src, int w, int h, int& padd_w_, int& padd_h_)
    {
        int in_w = src.cols;  // width
        int in_h = src.rows;  // height
        int tar_w = w;
        int tar_h = h;
        float r = std::min(float(tar_h) / in_h, float(tar_w) / in_w);
        int inside_w = round(in_w * r);
        int inside_h = round(in_h * r);
        padd_w_ = tar_w - inside_w;
        padd_h_ = tar_h - inside_h;

        cv::Mat resize_img;

        cv::resize(src, resize_img, cv::Size(inside_w, inside_h));

        padd_w_ = padd_w_ / 2;
        padd_h_ = padd_h_ / 2;

        int top = int(round(padd_h_ - 0.1));
        int bottom = int(round(padd_h_ + 0.1));
        int left = int(round(padd_w_ - 0.1));
        int right = int(round(padd_w_ + 0.1));

        cv::copyMakeBorder(
            resize_img, resize_img, top, bottom, left, right, 0, cv::Scalar(114, 114, 114));
        return resize_img;
    };

    cv::Mat YoloModel::letterbox(cv::Mat& src, int w, int h)
    {
        int _w, _h;
        return letterbox(src, w, h, _w, _h);
    };

    YoloModel::YoloModel(std::string model_path, int image_size)
        :image_size(image_size)
    {
        const auto explicit_batch = 1U << static_cast<uint32_t>(nvinfer1::NetworkDefinitionCreationFlag::kEXPLICIT_BATCH);
        std::unique_ptr<nvinfer1::IBuilder, TrtDeleter<nvinfer1::IBuilder>> builder(nvinfer1::createInferBuilder(logger_));
        if (!builder) throw std::runtime_error("TensorRT createInferBuilder failed");
        std::unique_ptr<nvinfer1::INetworkDefinition, TrtDeleter<nvinfer1::INetworkDefinition>> network(
            builder->createNetworkV2(explicit_batch));
        if (!network) throw std::runtime_error("TensorRT createNetworkV2 failed");
        std::unique_ptr<nvonnxparser::IParser, TrtDeleter<nvonnxparser::IParser>> parser(
            nvonnxparser::createParser(*network, logger_));
        if (!parser) throw std::runtime_error("TensorRT createParser failed");
        if (!parser->parseFromFile(model_path.c_str(), static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
            throw std::runtime_error("TensorRT parse ONNX failed: " + model_path);
        }
        std::unique_ptr<nvinfer1::IBuilderConfig, TrtDeleter<nvinfer1::IBuilderConfig>> config(builder->createBuilderConfig());
        if (!config) throw std::runtime_error("TensorRT createBuilderConfig failed");
#if NV_TENSORRT_MAJOR >= 8
        config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, 1ULL << 30);
#else
        config->setMaxWorkspaceSize(1ULL << 30);
#endif
        if (builder->platformHasFastFp16()) config->setFlag(nvinfer1::BuilderFlag::kFP16);
        std::unique_ptr<nvinfer1::IHostMemory, TrtDeleter<nvinfer1::IHostMemory>> serialized(
            builder->buildSerializedNetwork(*network, *config));
        if (!serialized) throw std::runtime_error("TensorRT buildSerializedNetwork failed");
        runtime_.reset(nvinfer1::createInferRuntime(logger_));
        if (!runtime_) throw std::runtime_error("TensorRT createInferRuntime failed");
        engine_.reset(runtime_->deserializeCudaEngine(serialized->data(), serialized->size()));
        if (!engine_) throw std::runtime_error("TensorRT deserializeCudaEngine failed");
        context_.reset(engine_->createExecutionContext());
        if (!context_) throw std::runtime_error("TensorRT createExecutionContext failed");
        if (engine_->getNbBindings() != 2) throw std::runtime_error("TensorRT expects exactly 2 bindings");
        for (int i = 0; i < engine_->getNbBindings(); i++) {
            if (engine_->bindingIsInput(i)) input_index_ = i;
            else output_index_ = i;
        }
        if (input_index_ < 0 || output_index_ < 0) throw std::runtime_error("TensorRT binding index error");

        auto input_dims = context_->getBindingDimensions(input_index_);
        if (input_dims.nbDims == 4 &&
            (input_dims.d[0] == -1 || input_dims.d[2] == -1 || input_dims.d[3] == -1)) {
            context_->setBindingDimensions(input_index_, nvinfer1::Dims4(1, 3, image_size, image_size));
        }
        if (!context_->allInputDimensionsSpecified()) {
            throw std::runtime_error("TensorRT input dimensions not specified");
        }

        auto calc_size = [](const nvinfer1::Dims& dims) {
            size_t vol = 1;
            for (int i = 0; i < dims.nbDims; i++) vol *= static_cast<size_t>(dims.d[i]);
            return vol * sizeof(float);
            };
        input_buffer_size_ = calc_size(context_->getBindingDimensions(input_index_));
        output_buffer_size_ = calc_size(context_->getBindingDimensions(output_index_));
        if (cudaStreamCreate(&stream_) != cudaSuccess) throw std::runtime_error("cudaStreamCreate failed");
        if (cudaMalloc(&device_buffers_[input_index_], input_buffer_size_) != cudaSuccess)
            throw std::runtime_error("cudaMalloc input failed");
        if (cudaMalloc(&device_buffers_[output_index_], output_buffer_size_) != cudaSuccess)
            throw std::runtime_error("cudaMalloc output failed");
    };

    YoloModel::~YoloModel()
    {
        if (device_buffers_[0] != nullptr) {
            cudaError_t err = cudaFree(device_buffers_[0]);
            if (err != cudaSuccess) {
                std::cerr << "cudaFree buffer0 failed: " << cudaGetErrorString(err) << std::endl;
            }
        }
        if (device_buffers_[1] != nullptr) {
            cudaError_t err = cudaFree(device_buffers_[1]);
            if (err != cudaSuccess) {
                std::cerr << "cudaFree buffer1 failed: " << cudaGetErrorString(err) << std::endl;
            }
        }
        if (stream_ != nullptr) {
            cudaError_t err = cudaStreamDestroy(stream_);
            if (err != cudaSuccess) {
                std::cerr << "cudaStreamDestroy failed: " << cudaGetErrorString(err) << std::endl;
            }
        }
    }

    void YoloModel::set_enemy_color(bool enemy_blue)
    {
        this->enemy_blue = enemy_blue;
    };

    std::vector<Armor> YoloModel::find_armors(cv::Mat src)
    {
        // ����������
        std::vector<bbox_t> bbox_ts = forward(src);
        // �޳���ԵĿ��
        bbox_ts = screen_out_edge_targets(bbox_ts, src.cols, src.rows);
        std::vector<Armor> out_armors;
        for (const auto& x : bbox_ts) {
            out_armors.push_back(Armor(x.pts, x.label));
        };
        return out_armors;
    }
    ;

    std::vector<YoloModel::bbox_t> YoloModel::forward(cv::Mat src)
    {
        // �趨
        double image_width = src.cols;
        double image_height = src.rows;
        src = letterbox(src, image_size, image_size, padd_w_, padd_h_);
        // ת����ɫ�ռ�
        cv::cvtColor(src, src, cv::COLOR_BGR2RGB);
        src.convertTo(src, CV_32F, 1.0 / 255.0);
        // ����ͨ�����������ݵ��������
        std::vector<cv::Mat> channels(3);
        cv::split(src, channels);
        std::vector<float> input_host(input_buffer_size_ / sizeof(float));
        float* input_data_host = input_host.data();
        int image_area = src.rows * src.cols;
        std::copy(channels[0].begin<float>(), channels[0].end<float>(), input_data_host + image_area * 0);
        std::copy(channels[1].begin<float>(), channels[1].end<float>(), input_data_host + image_area * 1);
        std::copy(channels[2].begin<float>(), channels[2].end<float>(), input_data_host + image_area * 2);
        cudaError_t cuda_err = cudaMemcpyAsync(device_buffers_[input_index_], input_data_host, input_buffer_size_,
            cudaMemcpyHostToDevice, stream_);
        if (cuda_err != cudaSuccess) {
            throw std::runtime_error(std::string("cudaMemcpyAsync H2D failed: ") + cudaGetErrorString(cuda_err));
        }
        if (!context_->enqueueV2(device_buffers_, stream_, nullptr)) {
            throw std::runtime_error("TensorRT enqueueV2 failed");
        }
        std::vector<float> output_host(output_buffer_size_ / sizeof(float));
        cuda_err = cudaMemcpyAsync(output_host.data(), device_buffers_[output_index_], output_buffer_size_,
            cudaMemcpyDeviceToHost, stream_);
        if (cuda_err != cudaSuccess) {
            throw std::runtime_error(std::string("cudaMemcpyAsync D2H failed: ") + cudaGetErrorString(cuda_err));
        }
        cuda_err = cudaStreamSynchronize(stream_);
        if (cuda_err != cudaSuccess) {
            throw std::runtime_error(std::string("cudaStreamSynchronize failed: ") + cudaGetErrorString(cuda_err));
        }

        float confidence_threshold = 0.25;

        auto output_dims = context_->getBindingDimensions(output_index_);
        if (output_dims.nbDims != 3) {
            throw std::runtime_error("Unexpected TensorRT output dims");
        }
        int output_numbox = output_dims.d[1];
        int output_numprob = output_dims.d[2];
        int modle_last_length = 13;
        int num_classes = output_numprob - modle_last_length; // 36
        float* output_buffer = output_host.data();
        int TOPK_NUM = output_numbox;

        // ���ĵ�ģ�Ͳ��� 49: �ĵ�ֱ�������,����,����,����
        // x0 y0 x1 y1 confince ltx lty lbx lby rbx rby rtx rty ==> 0 - 12
        // 13 - 48 ����,�ܹ�36��
        std::vector<bbox_t> rst;
        rst.reserve(TOPK_NUM);
        std::vector<uint8_t> removed(TOPK_NUM);
        for (int i = 0; i < TOPK_NUM; i++) {
            // ��ȡÿһ��i�����ݵ�λ��Ϣ
            auto* box_buffer = output_buffer + i * output_numprob;
            // ����4Ϊconfince���Ŷ�
            if (box_buffer[4] < confidence_threshold) continue;
            if (removed[i]) continue;
            rst.emplace_back();
            auto& box = rst.back();
            // box_buffer + 5 λ�Ƶ��õ�ַ
            memcpy(&box.pts, box_buffer + 5, 8 * sizeof(float));
            for (auto& pt : box.pts) {
                pt.x = (pt.x - padd_w_) / (image_size - 2 * padd_w_) * image_width;
                pt.y = (pt.y - padd_h_) / (image_size - 2 * padd_h_) * image_height;
            };
            box.confidence = sigmoid(box_buffer[4]);//prob * objness;
            // ���͵�ָ��Ϊ 13֮��
            float* pclass = box_buffer + modle_last_length;
            box.label = argmax(pclass, num_classes);
            for (int j = i + 1; j < TOPK_NUM; j++) {
                auto* box2_buffer = output_buffer + j * output_numprob;
                if (box2_buffer[4] < confidence_threshold) continue;
                if (removed[j]) continue;
                if (is_overlap(box_buffer, box2_buffer)) removed[j] = true;
            };
        };

        std::vector<bbox_t> out_rst;
        // label����
        for (const auto& rst_ : rst)
            if (strip_filter(rst_.label, this->enemy_blue))
                out_rst.push_back(rst_);
        return out_rst;
    };

    std::vector<YoloModel::bbox_t> YoloModel::screen_out_edge_targets(const std::vector<bbox_t>& bbox_ts,
        double image_width, double image_height)
    {
        std::vector<bbox_t> Out;
        for (const auto& x : bbox_ts) {
            if (
                MIN(x.pts[0].x, x.pts[1].x) <= 20 ||
                MIN(x.pts[0].y, x.pts[3].y) <= 20 ||
                MAX(x.pts[2].x, x.pts[3].x) >= image_width - 20 ||
                MAX(x.pts[2].y, x.pts[1].y) >= image_height - 20
                ) continue;
            Out.push_back(x);
        }
        return Out;
    };

}
