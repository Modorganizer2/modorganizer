#ifndef MODLISBYPRIORITYPROXY_H
#define MODLISBYPRIORITYPROXY_H

#include <optional>
#include <set>
#include <vector>

#include <QAbstractProxyModel>
#include <QModelIndex>
#include <QMultiHash>
#include <QStringList>
#include <QIcon>
#include <QSet>

#include "modinfo.h"

class ModList;
class Profile;

class ModListByPriorityProxy : public QAbstractProxyModel
{
  Q_OBJECT

public:
  explicit ModListByPriorityProxy(Profile* profile, QObject* parent = nullptr);
  ~ModListByPriorityProxy();

  void setProfile(Profile* profile) { m_Profile = profile; }

  void setSourceModel(QAbstractItemModel* sourceModel) override;

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  Qt::ItemFlags flags(const QModelIndex& idx) const override;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
  int columnCount(const QModelIndex& index) const override;
  bool hasChildren(const QModelIndex& parent) const override;

  bool setData(const QModelIndex& index, const QVariant& value, int role) override;

  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;

signals:
  void expandItem(const QModelIndex& index);

public slots:

  void expanded(const QModelIndex& index);
  void collapsed(const QModelIndex& index);

private:

  void buildTree();
  void expandItems(const QModelIndex& index);

  struct TreeItem {
    ModInfo::Ptr mod;
    unsigned int index;
    std::vector<std::unique_ptr<TreeItem>> children;
    TreeItem* parent;

    std::size_t childIndex(TreeItem* child) const {
      for (std::size_t i = 0; i < children.size(); ++i) {
        if (children[i].get() == child) {
          return i;
        }
      }
      return -1;
    }

    TreeItem() : TreeItem(nullptr, -1) { }
    TreeItem(ModInfo::Ptr mod, unsigned int index, TreeItem* parent = nullptr) :
      mod(mod), index(index), parent(parent) { }
  };

  TreeItem m_Root;
  std::map<unsigned int, TreeItem*> m_IndexToItem;
  std::set<QString> m_CollapsedItems;

private:
  Profile* m_Profile;
};

#endif //GROUPINGPROXY_H