#include "modernSignalChainMutationController.h"

#include "modernSignalChainSerializer.h"

namespace {
using Model = modernSignalChainModel;

QList<Model::Entry> *regionEntries(Model::ChainSnapshot *snapshot,
                                   Model::ChainRegion region)
{
    switch (region) {
    case Model::ChainRegion::CommonPrefix: return &snapshot->commonPrefix;
    case Model::ChainRegion::PathA: return &snapshot->pathA;
    case Model::ChainRegion::PathB: return &snapshot->pathB;
    case Model::ChainRegion::CommonSuffix: return &snapshot->commonSuffix;
    }
    return nullptr;
}

void setRegion(Model::Entry *entry, Model::ChainRegion region)
{
    entry->region = region;
    entry->path = region == Model::ChainRegion::PathA ? Model::PathA
        : region == Model::ChainRegion::PathB ? Model::PathB : Model::Common;
    entry->rawValue = modernSignalChainSerializer::rawForModule(
        entry->moduleId, region);
}

void updatePositions(Model::ChainSnapshot *snapshot)
{
    int position = 0;
    const auto update = [&position](QList<Model::Entry> *entries) {
        for (Model::Entry &entry : *entries)
            entry.originalPosition = position++;
    };
    update(&snapshot->commonPrefix);
    snapshot->split.originalPosition = position++;
    update(&snapshot->pathA);
    update(&snapshot->pathB);
    snapshot->merge.originalPosition = position++;
    update(&snapshot->commonSuffix);
}
}

modernSignalChainMutationController::modernSignalChainMutationController(
    modernSignalChainModel *model)
    : chainModel(model)
{
}

ChainMoveResult modernSignalChainMutationController::moveModule(
    int moduleId, modernSignalChainModel::ChainRegion destinationRegion,
    int destinationIndex)
{
    ChainMoveResult result;
    if (!chainModel || !chainModel->isValid()) {
        result.error = "Signal chain model is unavailable";
        return result;
    }

    result.before = chainModel->snapshot();
    result.after = result.before;
    if (!Model::isMovableModule(moduleId)) {
        result.error = QString("Module %1 is fixed and cannot be moved").arg(moduleId);
        return result;
    }

    QList<Model::Entry> *source = nullptr;
    Model::ChainRegion sourceRegion = Model::ChainRegion::CommonPrefix;
    int sourceIndex = -1;
    const Model::ChainRegion regions[] = {
        Model::ChainRegion::CommonPrefix, Model::ChainRegion::PathA,
        Model::ChainRegion::PathB, Model::ChainRegion::CommonSuffix
    };
    for (Model::ChainRegion region : regions) {
        QList<Model::Entry> *entries = regionEntries(&result.after, region);
        for (int index = 0; index < entries->size(); ++index) {
            if (entries->at(index).moduleId == moduleId) {
                source = entries;
                sourceRegion = region;
                sourceIndex = index;
                break;
            }
        }
        if (source)
            break;
    }
    if (!source) {
        result.error = QString("Module %1 was not found in editable regions")
                           .arg(moduleId);
        return result;
    }

    QList<Model::Entry> *destination = regionEntries(&result.after,
                                                      destinationRegion);
    if (!destination || destinationIndex < 0
        || destinationIndex > destination->size()) {
        result.error = QString("Destination index %1 is outside the target region")
                           .arg(destinationIndex);
        return result;
    }

    Model::Entry moved = source->takeAt(sourceIndex);
    if (sourceRegion == destinationRegion && sourceIndex < destinationIndex)
        --destinationIndex;
    setRegion(&moved, destinationRegion);
    destination->insert(destinationIndex, moved);
    result.after.revision = result.before.revision + 1;
    updatePositions(&result.after);

    QString validationError;
    if (!modernSignalChainSerializer::serialize(
            result.after, &result.serializedBytes, &validationError)
        || !chainModel->replaceSnapshot(result.after, &validationError)) {
        result.error = validationError;
        result.after = result.before;
        result.serializedBytes.clear();
        return result;
    }

    result.accepted = true;
    return result;
}
