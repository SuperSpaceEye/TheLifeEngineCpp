//
// Created by spaceeye on 01.04.2022.
//

#ifndef THELIFEENGINECPP_ORGANISMBLOCKPARAMETERS_H
#define THELIFEENGINECPP_ORGANISMBLOCKPARAMETERS_H

struct BlockParameters {
    // food_cost_modifier - how much food does organism have to spend on one block when creating a child
    // life_point_amount - how much organism gains life points from this block
    float food_cost_modifier = 1; float life_point_amount = 1; float chance_weight = 1;
};

struct OrganismBlockParameters {
    BlockParameters MouthBlock    = {1, 1, 1};
    BlockParameters ProducerBlock = {1, 1, 1};
    BlockParameters MoverBlock    = {1, 1, 1};
    BlockParameters KillerBlock   = {1, 1, 1};
    BlockParameters ArmorBlock    = {1, 1, 1};
    BlockParameters EyeBlock      = {1, 1, 1};
};

enum class BlocksNames {
    MouthBlock,
    ProducerBlock,
    MoverBlock,
    KillerBlock,
    ArmorBlock,
    EyeBlock
};

enum class ParametersNames {
    FoodCostModifier,
    LifePointAmount,
    ChanceWeight,
};

#endif //THELIFEENGINECPP_ORGANISMBLOCKPARAMETERS_H