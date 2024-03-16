/*

Copyright (c) 2018, John Smith

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted, provided that the
above copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR
BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/


#include "OrphanFieldsModel.h"

OrphanFieldsModel::OrphanFieldsModel(QObject *parent)
    : QAbstractTableModel(parent)
{

}


int OrphanFieldsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return (int)size();
}


int OrphanFieldsModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return ColumnCount;
}

QVariant OrphanFieldsModel::data(const QModelIndex &index, int role) const {
    if (role == Qt::DisplayRole) {
        const auto &frame = std::next(cbegin(), index.row());

        if (index.column() == FrameColumn)
            return QVariant(frame->first);
        else if (index.column() == MatchColumn)
            return QVariant(QString(frame->second.match));
        else if (index.column() == DecimationColumn)
            return QVariant(QString(frame->second.decimated ? "Yes" : "No"));
    }

    return QVariant();
}


QVariant OrphanFieldsModel::headerData(int section, Qt::Orientation orientation, int role) const {
    const char *column_headers[ColumnCount] = {
        "Frame",
        "Type",
        "Decimated",
    };

    if (role == Qt::DisplayRole) {
        if (orientation == Qt::Horizontal) {
            return QVariant(QString(column_headers[section]));
        } else if (orientation == Qt::Vertical) {
            return QVariant(section + 1);
        }
    }

    return QVariant();
}


void OrphanFieldsModel::insert(const value_type &orphan) {
    OrphanFieldMap::const_iterator it = lower_bound(orphan.first);

    if (it != cend() && it->first == orphan.first)
        return;

    int new_row = 0;
    if (size())
        new_row = (int)std::distance(cbegin(), it);

    beginInsertRows(QModelIndex(), new_row, new_row);

    OrphanFieldMap::insert(it, orphan);

    endInsertRows();
}


void OrphanFieldsModel::erase(int frame) {
    OrphanFieldMap::const_iterator it = find(frame);

    if (it == cend())
        return;

    int row = (int)std::distance(cbegin(), it);

    beginRemoveRows(QModelIndex(), row, row);

    OrphanFieldMap::erase(it);

    endRemoveRows();
}


void OrphanFieldsModel::clear() {
    if (!size())
        return;

    beginRemoveRows(QModelIndex(), 0, size() - 1);

    OrphanFieldMap::clear();

    endRemoveRows();
}
