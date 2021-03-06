/** ****************************************************************************
 *  @file    Forest.hpp
 *  @brief   Real-time facial feature detection
 *  @author  Roberto Valle Fernandez
 *  @date    2015/06
 *  @copyright All rights reserved.
 *  Software developed by UPM PCR Group: http://www.dia.fi.upm.es/~pcr
 ******************************************************************************/

// ------------------ RECURSION PROTECTION -------------------------------------
#ifndef FOREST_HPP
#define FOREST_HPP

// ----------------------- INCLUDES --------------------------------------------
#include <trace.hpp>
#include <Tree.hpp>
#include <cereal/access.hpp>
#include <cereal/types/vector.hpp>

/** ****************************************************************************
 * @class Forest
 * @brief Conditional regression forest
 ******************************************************************************/
template<typename Sample>
class Forest
{
public:
  typedef typename Sample::Split Split;
  typedef typename Sample::Leaf Leaf;

  Forest() {};

  void
  addTree
    (
    std::shared_ptr<Tree<Sample>> tree
    )
  {
    m_trees.push_back(tree);
  };

  std::shared_ptr<Tree<Sample>>
  getTree
    (
    int idx
    )
  {
    return m_trees[idx];
  };

  int
  numberOfTrees() const
  {
    return static_cast<int>(m_trees.size());
  };

  void
  cleanForest()
  {
    m_trees.clear();
  };

  void
  evaluateMT
    (
    const std::shared_ptr<Sample> sample,
    std::shared_ptr<Leaf> *leaf
    ) const
  {
    for (const std::shared_ptr<Tree<Sample>> &tree : m_trees)
    {
      tree->evaluateMT(sample, tree->root, leaf);
      leaf++;
    }
  };

  bool
  load
    (
    std::string path,
    ForestParam fp,
    int max_trees = -1
    )
  {
    setParam(fp);
    if (max_trees == -1)
      max_trees = fp.ntrees;
    UPM_PRINT("> Trees to load: " << fp.ntrees);

    for (int i=0; i < fp.ntrees; i++)
    {
      if (numberOfTrees() > max_trees)
        continue;

      char buffer[200];
      sprintf(buffer, "%stree_%03d.bin", path.c_str(), i);
      std::string tree_path = buffer;
      UPM_PRINT("  Load " << tree_path);
      if (not load_tree(tree_path, m_trees))
        return false;
    }
    return true;
  };

  static bool
  load_tree
    (
    std::string url,
    std::vector<std::shared_ptr<Tree<Sample>>> &trees
    )
  {
    std::shared_ptr<Tree<Sample>> tree;
    if (not Tree<Sample>::load(tree, url))
      return false;

    if (tree->isFinished())
    {
      trees.push_back(tree);
      return true;
    }
    else
    {
      UPM_PRINT("  Tree is not finished successfully")
      return false;
    }
  };

  void
  setParam
    (
    ForestParam fp
    )
  {
    m_forest_param = fp;
  };

  ForestParam
  getParam() const
  {
    return m_forest_param;
  };

private:
  std::vector<std::shared_ptr<Tree<Sample>>> m_trees;
  ForestParam m_forest_param;

  friend class cereal::access;
  template<class Archive>
  void serialize(Archive &ar, const unsigned int version)
  {
    ar & m_trees;
  }
};

#endif /* FOREST_HPP */
