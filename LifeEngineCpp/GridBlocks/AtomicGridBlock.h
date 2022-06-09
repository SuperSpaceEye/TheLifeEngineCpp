//
// Created by spaceeye on 28.05.2022.
//

#ifndef THELIFEENGINECPP_ATOMICGRIDBLOCK_H
#define THELIFEENGINECPP_ATOMICGRIDBLOCK_H

#include <atomic>
#include "BaseGridBlock.h"

struct AtomicGridBlock {
    std::atomic<bool> locked{false};

    BlockTypes type = BlockTypes::EmptyBlock;
    Rotation rotation = Rotation::UP;
    Neighbors neighbors{};

    Organism * organism = nullptr;
    AtomicGridBlock()=default;
    AtomicGridBlock(const AtomicGridBlock & block): type(block.type),
                                                    rotation(block.rotation),
                                                    neighbors(block.neighbors),
                                                    organism(block.organism) {}
    explicit AtomicGridBlock(BlockTypes type, Rotation rotation = Rotation::UP, Neighbors neighbors = Neighbors{},
            Organism * organism = nullptr):
    type(type), rotation(rotation), neighbors(neighbors), organism(organism) {}
    AtomicGridBlock& operator=(const AtomicGridBlock & block) {
        type = block.type;
        rotation = block.rotation;
        neighbors = block.neighbors;
        organism = block.organism;
        return *this;
    };
};


#endif //THELIFEENGINECPP_ATOMICGRIDBLOCK_H
