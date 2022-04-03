//
// Created by DefTruth on 2022/3/27.
//

#include "mnn_insectid.h"
#include "lite/utils.h"

using mnncv::MNNInsectID;

MNNInsectID::MNNInsectID(const std::string &_mnn_path, unsigned int _num_threads)
    : BasicMNNHandler(_mnn_path, _num_threads)
{
  initialize_pretreat();
}

inline void MNNInsectID::initialize_pretreat()
{
  pretreat = std::shared_ptr<MNN::CV::ImageProcess>(
      MNN::CV::ImageProcess::create(
          MNN::CV::BGR,
          MNN::CV::RGB,
          mean_vals, 3,
          norm_vals, 3
      )
  );
}

inline void MNNInsectID::transform(const cv::Mat &mat)
{
  cv::Mat canvas;
  cv::resize(mat, canvas, cv::Size(input_width, input_height));
  // (1,3,224,224)
  pretreat->convert(canvas.data, input_width, input_height, canvas.step[0], input_tensor);
}

void MNNInsectID::detect(const cv::Mat &mat, types::ImageNetContent &content, unsigned int top_k)
{
  if (mat.empty()) return;
  // 1. make input tensor
  this->transform(mat);
  // 2. inference
  mnn_interpreter->runSession(mnn_session);
  auto output_tensors = mnn_interpreter->getSessionOutputAll(mnn_session);
  // 3. fetch.
  auto device_logits_ptr = output_tensors.at("477");
  MNN::Tensor host_logits_tensor(device_logits_ptr, device_logits_ptr->getDimensionType());
  device_logits_ptr->copyToHostTensor(&host_logits_tensor);

  auto logits_dims = host_logits_tensor.shape();
  const unsigned int num_classes = logits_dims.at(1); // 1000
  const float *logits = host_logits_tensor.host<float>();

  unsigned int max_id;
  std::vector<float> scores = lite::utils::math::softmax<float>(logits, num_classes, max_id);
  std::vector<unsigned int> sorted_indices = lite::utils::math::argsort<float>(scores);
  if (top_k > num_classes) top_k = num_classes;

  content.scores.clear();
  content.labels.clear();
  content.texts.clear();
  for (unsigned int i = 0; i < top_k; ++i)
  {
    content.labels.push_back(sorted_indices[i]);
    content.scores.push_back(scores[sorted_indices[i]]);
    content.texts.push_back(class_names[sorted_indices[i]]);
  }
  content.flag = true;
}