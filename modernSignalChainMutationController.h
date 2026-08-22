#ifndef MODERNSIGNALCHAINMUTATIONCONTROLLER_H
#define MODERNSIGNALCHAINMUTATIONCONTROLLER_H

#include "modernSignalChainModel.h"

struct ChainMoveResult
{
    bool accepted = false;
    QString error;
    modernSignalChainModel::ChainSnapshot before;
    modernSignalChainModel::ChainSnapshot after;
    QList<QString> serializedBytes;
};

class modernSignalChainMutationController
{
public:
    explicit modernSignalChainMutationController(modernSignalChainModel *model);

    ChainMoveResult moveModule(
        int moduleId,
        modernSignalChainModel::ChainRegion destinationRegion,
        int destinationIndex);

private:
    modernSignalChainModel *chainModel = nullptr;
};

#endif
