/** ****************************************************************************
 *  @file    Tree.hpp
 *  @brief   Real-time facial feature detection
 *  @author  Roberto Valle Fernandez
 *  @date    2015/06
 *  @copyright All rights reserved.
 *  Software developed by UPM PCR Group: http://www.dia.fi.upm.es/~pcr
 ******************************************************************************/

// ------------------ RECURSION PROTECTION -------------------------------------
#ifndef TREE_HPP
#define TREE_HPP

// ----------------------- INCLUDES --------------------------------------------
#include <trace.hpp>
#include <Constants.hpp>
#include <SplitGen.hpp>
#include <TreeNode.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <cereal/access.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>

/** ****************************************************************************
 * @class Tree
 * @brief Conditional regression tree
 ******************************************************************************/
template<typename Sample>
class Tree
{
public:
  typedef typename Sample::Split Split;
  typedef typename Sample::Leaf Leaf;

  Tree() {};

  Tree
    (
    const std::vector<std::shared_ptr<Sample>> &samples,
    ForestParam param,
    boost::mt19937 *rng,
    std::string save_path
    )
  {
    m_rng = rng;
    m_save_path = save_path;
    m_param = param;
    m_num_nodes = powf(2.0f, m_param.max_depth) - 1;
    i_node = 0;
    i_leaf = 0;

    UPM_PRINT("Start training");
    m_ticks = static_cast<double>(cv::getTickCount());
    root = std::make_shared<TreeNode<Sample>>(TreeNode<Sample>(0));
    grow(root, samples);
    save();
  };

  virtual
  ~Tree() {};

  bool
  isFinished()
  {
    if (m_num_nodes == 0)
      return false;
    return i_node == m_num_nodes;
  };

  void
  update
    (
    const std::vector<std::shared_ptr<Sample>> &samples,
    boost::mt19937* rng
    )
  {
    m_rng = rng;
    UPM_PRINT(100*static_cast<float>(i_node)/static_cast<float>(m_num_nodes) << "% : update tree");
    if (not isFinished())
    {
      i_node = 0;
      i_leaf = 0;

      UPM_PRINT("Start training");
      m_ticks = static_cast<double>(cv::getTickCount());
      grow(root, samples);
      save();
    }
  };

  void
  grow
    (
    std::shared_ptr<TreeNode<Sample>> node,
    const std::vector<std::shared_ptr<Sample>> &samples
    )
  {
    const int depth = node->getDepth();
    const int nelements = static_cast<int>(samples.size());
    // Stop growing a tree when the depth reaches 15 or if there are less than
    // 20 patches left for training
    if (nelements < m_param.min_patches or depth >= m_param.max_depth or node->isLeaf())
    {
      node->createLeaf(samples);
      i_node += powf(2.0f, m_param.max_depth-depth) - 1;
      i_leaf++;
      UPM_PRINT("  (1) " << 100*static_cast<float>(i_node)/static_cast<float>(m_num_nodes)
            << "% : leaf(depth: " << depth << ", elements: " << nelements
            << ") [i_leaf: " << i_leaf << "]");
    }
    else
    {
      if (node->isSplit())
      {
        // Only in reload mode
        Split best_split = node->getSplit();
        std::vector<std::vector<std::shared_ptr<Sample>>> sets;
        applyOptimalSplit(samples, best_split, sets);
        i_node++;
        UPM_PRINT("  (2) " << 100*static_cast<float>(i_node)/static_cast<float>(m_num_nodes)
              << "% : split(depth: " << depth << ", elements: " << nelements
              << ") [A: " << sets[0].size() << ", B: " << sets[1].size() << "]");

        grow(node->left, sets[0]);
        grow(node->right, sets[1]);
      }
      else
      {
        Split best_split;
        if (findOptimalSplit(samples, best_split))
        {
          std::vector<std::vector<std::shared_ptr<Sample>>> sets;
          applyOptimalSplit(samples, best_split, sets);
          node->createSplit(best_split);
          i_node++;

          std::shared_ptr<TreeNode<Sample>> left = std::make_shared<TreeNode<Sample>>(TreeNode<Sample>(depth+1));
          node->addLeftChild(left);

          std::shared_ptr<TreeNode<Sample>> right = std::make_shared<TreeNode<Sample>>(TreeNode<Sample>(depth+1));
          node->addRightChild(right);

          saveAuto();
          UPM_PRINT("  (3) " << 100*static_cast<float>(i_node)/static_cast<float>(m_num_nodes)
                << "% : split(depth: " << depth << ", elements: " << nelements
                << ") [A: " << sets[0].size() << ", B: " << sets[1].size() << "]");

          grow(left, sets[0]);
          grow(right, sets[1]);
        }
        else
        {
          UPM_PRINT("  No valid split found");
          node->createLeaf(samples);
          i_node += powf(2.0f, m_param.max_depth-depth) - 1;
          i_leaf++;
          UPM_PRINT("  (4) " << 100*static_cast<float>(i_node)/static_cast<float>(m_num_nodes)
                << "% : leaf(depth: " << depth << ", elements: "  << nelements
                << ") [i_leaf: " << i_leaf << "]");
        }
      }
    }
  };

  static void
  evaluateMT
    (
    const std::shared_ptr<Sample> sample,
    std::shared_ptr<TreeNode<Sample>> node,
    std::shared_ptr<Leaf> *leaf
    )
  {
    if (node->isLeaf())
      *leaf = node->getLeaf();
    else
    {
      if (node->eval(sample))
        evaluateMT(sample, node->left, leaf);
      else
        evaluateMT(sample, node->right, leaf);
    }
  };

  static bool
  load
    (
    std::shared_ptr<Tree> &tree,
    std::string path
    )
  {
    // Check if file exist
    std::ifstream ifs(path.c_str());
    if (not ifs.is_open())
    {
      UPM_PRINT("  File not found: " << path);
      return false;
    }

    try
    {
      cereal::BinaryInputArchive ia(ifs);
      Tree *tree_aux = new Tree();
      ia >> *tree_aux;
      tree = std::make_shared<Tree>(*tree_aux);
      if (tree->isFinished())
      {
        UPM_PRINT("  Complete tree reloaded");
      }
      else
      {
        UPM_PRINT("  Unfinished tree reloaded");
      }
      ifs.close();
      return true;
    }
    catch (cereal::Exception &ex)
    {
      UPM_ERROR("  Exception during tree serialization: " << ex.what());
      return false;
    }
  };

  void
  save()
  {
    try
    {
      std::ofstream ofs(m_save_path.c_str());
      cereal::BinaryOutputArchive oa(ofs);
      oa << *this; // it can also save unfinished trees
      ofs.flush();
      ofs.close();
      UPM_PRINT("Complete tree saved: " << m_save_path);
    }
    catch (cereal::Exception &ex)
    {
      UPM_ERROR("Exception during tree serialization: " << ex.what());
    }
  };

  std::shared_ptr<TreeNode<Sample>> root; // root node of the tree

private:
  bool
  findOptimalSplit
    (
    const std::vector<std::shared_ptr<Sample>> &samples,
    Split &best_split
    )
  {
    // Number of tests to find the best split θ = (R1,R2,α)
    std::vector<Split> splits(m_param.ntests);
    SplitGen<Sample> sg(samples, splits, m_rng, m_param.patch_size, m_param.nthresholds);
    sg.generate();

    // Select the splitting which maximizes the information gain
    best_split.info = boost::numeric::bounds<double>::highest();
    for (unsigned int i=0; i < splits.size(); i++)
      if (splits[i].info < best_split.info)
        best_split = splits[i];

    if (best_split.info != boost::numeric::bounds<double>::highest())
      return true;

    return false;
  };

  void
  applyOptimalSplit
    (
    const std::vector<std::shared_ptr<Sample>> &samples,
    Split &best_split,
    std::vector<std::vector<std::shared_ptr<Sample>>> &sets
    )
  {
    // Process each patch with the optimal R1 and R2
    std::vector<IntIndex> val_set(samples.size());
    for (unsigned int i=0; i < samples.size(); ++i)
    {
      val_set[i].first  = samples[i]->evalTest(best_split);
      val_set[i].second = i;
    }
    std::sort(val_set.begin(), val_set.end());
    SplitGen<Sample>::splitSamples(samples, val_set, sets, best_split.threshold);
  };

  void
  saveAuto()
  {
    double ticks = static_cast<double>(cv::getTickCount());
    double time = (ticks-m_ticks)/cv::getTickFrequency();
    // Save every 10 minutes
    if (time > 600)
    {
      m_ticks = ticks;
      UPM_PRINT("Automatic tree saved at " << m_ticks);
      save();
    }
  };

  boost::mt19937 *m_rng;
  double m_ticks;
  int m_num_nodes;
  int i_node;
  int i_leaf;
  ForestParam m_param;
  std::string m_save_path;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & m_num_nodes;
    ar & i_node;
    ar & m_param;
    ar & m_save_path;
    ar & root;
  }
};

#endif /* TREE_HPP */
