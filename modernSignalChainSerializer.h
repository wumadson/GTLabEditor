#ifndef MODERNSIGNALCHAINSERIALIZER_H
#define MODERNSIGNALCHAINSERIALIZER_H

#include "modernSignalChainModel.h"

class modernSignalChainSerializer
{
public:
    static bool validate(const modernSignalChainModel::ChainSnapshot &snapshot,
                         QString *error = nullptr);
    static bool serialize(const modernSignalChainModel::ChainSnapshot &snapshot,
                          QList<QString> *bytes,
                          QString *error = nullptr);
    static QString rawForModule(int moduleId,
                                modernSignalChainModel::ChainRegion region);
};

#endif
