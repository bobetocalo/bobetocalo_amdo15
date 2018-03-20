/** ****************************************************************************
 *  @file    ImageSample.cpp
 *  @brief   Real-time facial feature detection
 *  @author  Roberto Valle Fernandez
 *  @date    2015/06
 *  @copyright All rights reserved.
 *  Software developed by UPM PCR Group: http://www.dia.fi.upm.es/~pcr
 ******************************************************************************/

// ----------------------- INCLUDES --------------------------------------------
#include <ImageSample.hpp>

ImageSample::ImageSample
  (
  const cv::Mat &img,
  std::vector<int> features,
  bool use_integral
  ) :
  m_use_integral(use_integral)
{
  sort(features.begin(), features.end());
  FeatureChannelFactory fcf = FeatureChannelFactory();
  for (unsigned int i=0; i < features.size(); i++)
    fcf.extractChannel(features[i], m_use_integral, img, m_feature_channels);
};

ImageSample::~ImageSample()
{
  for (unsigned int i=0; i < m_feature_channels.size(); i++)
    m_feature_channels[i].release();
  m_feature_channels.clear();
};

int
ImageSample::evalTest
  (
  const SimplePatchFeature &test,
  const cv::Rect rect
  ) const
{
  // Simple patch feature comparison
  int p1 = 0, p2 = 0;
  const cv::Mat img = m_feature_channels[test.feature_channel];
  if (m_use_integral)
  {
    int R1_a = img.at<float>(rect.y+test.rect1.y, rect.x+test.rect1.x);
    int R1_b = img.at<float>(rect.y+test.rect1.y, rect.x+test.rect1.x+test.rect1.width);
    int R1_c = img.at<float>(rect.y+test.rect1.y+test.rect1.height, rect.x+test.rect1.x);
    int R1_d = img.at<float>(rect.y+test.rect1.y+test.rect1.height, rect.x+test.rect1.x+test.rect1.width);
    p1 = (R1_d - R1_b - R1_c + R1_a) / static_cast<float>(test.rect1.width*test.rect1.height);

    int R2_a = img.at<float>(rect.y+test.rect2.y, rect.x+test.rect2.x);
    int R2_b = img.at<float>(rect.y+test.rect2.y, rect.x+test.rect2.x+test.rect2.width);
    int R2_c = img.at<float>(rect.y+test.rect2.y+test.rect2.height, rect.x+test.rect2.x);
    int R2_d = img.at<float>(rect.y+test.rect2.y+test.rect2.height, rect.x+test.rect2.x+test.rect2.width);
    p2 = (R2_d - R2_b - R2_c + R2_a) / static_cast<float>(test.rect2.width*test.rect2.height);
  }
  else
  {
    cv::Mat R1 = img(cv::Rect(test.rect1.x+rect.x, test.rect1.y+rect.y, test.rect1.width, test.rect1.height));
    p1 = (cv::sum(R1))[0] / static_cast<float>(test.rect1.width*test.rect1.height);

    cv::Mat R2 = img(cv::Rect(test.rect2.x+rect.x, test.rect2.y+rect.y, test.rect2.width, test.rect2.height));
    p2 = (cv::sum(R2))[0] / static_cast<float>(test.rect2.width*test.rect2.height);
  }

  return p1 - p2; // f_θ
};
