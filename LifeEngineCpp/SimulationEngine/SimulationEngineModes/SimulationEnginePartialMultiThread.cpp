//
// Created by spaceeye on 16.05.2022.
//

#include "SimulationEnginePartialMultiThread.h"

void SimulationEnginePartialMultiThread::partial_multi_thread_tick(EngineDataContainer *dc,
                                                                   EngineControlParameters *cp,
                                                                   OrganismBlockParameters *bp,
                                                                   SimulationParameters *sp,
                                                                   lehmer64 *gen) {
    place_organisms(dc, cp);

    if (sp->eat_then_produce) {
        start_stage(dc, PartialSimulationStage::EatFood);
        start_stage(dc, PartialSimulationStage::ProduceFood);
    } else {
        start_stage(dc, PartialSimulationStage::ProduceFood);
        start_stage(dc, PartialSimulationStage::EatFood);
    }

    if (sp->killer_damage_amount > 0) {
        start_stage(dc, PartialSimulationStage::ApplyDamage);}
    start_stage(dc, PartialSimulationStage::TickLifetime);

    erase_organisms(dc, dc->threaded_to_erase);

    for (int i = 0; i < dc->organisms_pools.size(); i++) {
        SimulationEngineSingleThread::reserve_observations(dc->pooled_organisms_observations[i], dc->organisms_pools[i],
                                                           sp, nullptr);
    }
    start_stage(dc, PartialSimulationStage::GetObservations);
    start_stage(dc, PartialSimulationStage::ThinkDecision);

    for (auto & pool: dc->organisms_pools) {
        for (auto & organism: pool)  {SimulationEngineSingleThread::make_decision(dc, sp, organism, gen);}
    }

    change_organisms_pools(dc, cp);

    for (auto & pool: dc->organisms_pools) {
        for (auto & organism: pool) {SimulationEngineSingleThread::try_make_child(dc, sp, organism, dc->to_place_organisms, gen);}
    }
}

void SimulationEnginePartialMultiThread::place_organisms(EngineDataContainer * dc, EngineControlParameters * cp) {
    for (auto & organism: dc->to_place_organisms) {
        place_organism(dc, organism);
        int pool = (organism->x / dc->simulation_width) * cp->num_threads;
        dc->organisms_pools[pool].emplace_back(organism);
    }
    dc->to_place_organisms.clear();
}

//TODO make smarter load balancer
void SimulationEnginePartialMultiThread::change_organisms_pools(EngineDataContainer *dc, EngineControlParameters * cp) {
    auto changes = std::vector<std::vector<Organism*>>(cp->num_threads);
    auto int_pos = std::vector<std::vector<int>>(cp->num_threads);
    //Go through all pools
    for (int num_pool = 0; num_pool < cp->num_threads; num_pool++) {
        //get pool reference
        auto &pool = dc->organisms_pools[num_pool];
        //go through all organisms in pool
        for (int pos_in_pool = 0; pos_in_pool < pool.size(); pos_in_pool++) {
            //get reference of organism in pool;
            auto &organism = pool[pos_in_pool];
            //calculate organism's pool position
            int organism_pool_pos = (float(organism->x) / dc->simulation_width) * cp->num_threads;
            //if organism no longer belongs to this pool.
            if (organism_pool_pos != num_pool) {
                changes[organism_pool_pos].emplace_back(organism);
                int_pos[num_pool].emplace_back(pos_in_pool);
            }
        }
    }

    for (int pool_num = 0; pool_num < dc->organisms_pools.size(); pool_num++) {
        auto & pool = dc->organisms_pools[pool_num];
        for (int i = 0; i < int_pos[pool_num].size(); i++) {
            pool.erase(pool.begin() + int_pos[pool_num][i] - i);
        }
    }

    for (int pool_num = 0; pool_num < dc->organisms_pools.size(); pool_num++) {
        auto & pool = dc->organisms_pools[pool_num];
        auto & to_place_pool = changes[pool_num];

        for (auto & organism : to_place_pool) {
            pool.emplace_back(organism);
        }
    }
}

void SimulationEnginePartialMultiThread::build_threads(EngineDataContainer &dc, EngineControlParameters &cp,
                                                       SimulationParameters &sp) {
    kill_threads(dc);
    dc.threads.reserve(cp.num_threads);
    dc.threaded_to_erase.clear();
    dc.threaded_to_erase.reserve(cp.num_threads);

    for (auto & pool: dc.organisms_pools) {
        for (auto & organism: pool) {
            dc.organisms.emplace_back(organism);
        }
    }

    dc.organisms_pools.clear();
    dc.organisms_pools.reserve(cp.num_threads);
    dc.pooled_organisms_observations.clear();
    dc.pooled_organisms_observations.reserve(cp.num_threads);
    for (int i = 0; i < cp.num_threads; i++) {
        dc.threaded_to_erase.emplace_back(std::vector<int>{});
        dc.organisms_pools.emplace_back(std::vector<Organism*>());
        dc.pooled_organisms_observations.emplace_back(std::vector<std::vector<Observation>>());
    }

    for (int i = 0; i < cp.num_threads; ++i) {
        dc.threads.emplace_back(eager_worker_partial{&dc, &sp, i});
    }

    for (auto & organism: dc.organisms) {
        dc.organisms_pools[0].emplace_back(organism);
    }
    change_organisms_pools(&dc, &cp);
    dc.organisms.clear();
    cp.build_threads = false;
}

void SimulationEnginePartialMultiThread::kill_threads(EngineDataContainer &dc) {
    if (!dc.threads.empty()) {
        for (auto & thread: dc.threads) {
            thread.stop_work();
        }
        dc.threads.clear();
    }
}

void SimulationEnginePartialMultiThread::start_stage(EngineDataContainer *dc, PartialSimulationStage stage) {
    for (auto & thread : dc->threads) {
        thread.work(stage);
    }

    for (auto & thread: dc->threads) {
        thread.finish();
    }

    if (stage == PartialSimulationStage::GetObservations) {
        for (auto & observation_pool: dc->pooled_organisms_observations) {
            observation_pool.clear();
        }
    }
}

void SimulationEnginePartialMultiThread::place_organism(EngineDataContainer *dc, Organism *organism) {
    for (auto &block: organism->organism_anatomy->_organism_blocks) {
        dc->CPU_simulation_grid[organism->x + block.get_pos(organism->rotation).x]
        [organism->y + block.get_pos(organism->rotation).y].type = block.type;
        dc->CPU_simulation_grid[organism->x + block.get_pos(organism->rotation).x]
        [organism->y + block.get_pos(organism->rotation).y].rotation = block.rotation;
        dc->CPU_simulation_grid[organism->x + block.get_pos(organism->rotation).x]
        [organism->y + block.get_pos(organism->rotation).y].organism = organism;
    }
}

//void SimulationEnginePartialMultiThread::produce_food(EngineDataContainer * dc, SimulationParameters * sp, Organism *organism, boost::mt19937 & gen) {
//    if (organism->organism_anatomy->_producer_blocks <= 0) {return;}
//    if (organism->organism_anatomy->_mover_blocks > 0 && !sp->movers_can_produce_food) {return;}
//    if (organism->lifetime % sp->produce_food_every_n_life_ticks != 0) {return;}
//
//    for (int i = 0; i < organism->organism_anatomy->_producer_blocks; i++) {
//        for (auto & pc: organism->organism_anatomy->_producing_space) {
//            //TODO locking here?
//            if (dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].type == EmptyBlock) {
//                if (std::uniform_real_distribution<float>(0, 1)(gen) < sp->food_production_probability) {
//                    dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].type = FoodBlock;
//                    break;
//                }
//            }
//        }
//    }
//}

//TODO this function takes like 40% of thread_tick work, so it needs redoing.
void SimulationEnginePartialMultiThread::eat_food(EngineDataContainer *dc, SimulationParameters *sp, Organism *organism) {
    for (auto & pc: organism->organism_anatomy->_eating_space) {
        while (dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].locked) {}
        dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].locked = true;
        if (dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].type == FoodBlock) {
            dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].type = EmptyBlock;
            organism->food_collected++;
        }
        dc->CPU_simulation_grid[organism->x + pc.get_pos(organism->rotation).x][organism->y + pc.get_pos(organism->rotation).y].locked = false;
    }
}

void SimulationEnginePartialMultiThread::tick_lifetime(EngineDataContainer *dc, std::vector<std::vector<int>> &to_erase,
                                                       Organism *organism, int thread_num, int organism_pos) {
    organism->lifetime++;
    if (organism->lifetime > organism->max_lifetime || organism->damage > organism->life_points) {
        for (auto & block: organism->organism_anatomy->_organism_blocks) {
            dc->CPU_simulation_grid[organism->x + block.get_pos(organism->rotation).x][organism->y + block.get_pos(organism->rotation).y].type = FoodBlock;
            dc->CPU_simulation_grid[organism->x + block.get_pos(organism->rotation).x][organism->y + block.get_pos(organism->rotation).y].organism = nullptr;
        }
        to_erase[thread_num].emplace_back(organism_pos);
    }
}

void SimulationEnginePartialMultiThread::erase_organisms(EngineDataContainer * dc, std::vector<std::vector<int>>& to_erase) {
    for (int pool_num = 0; pool_num < dc->organisms_pools.size(); pool_num++) {
        auto & thread_delete = dc->threaded_to_erase[pool_num];
        auto & pool = dc->organisms_pools[pool_num];
        for (int i = 0; i < thread_delete.size(); i++) {
            delete pool[thread_delete[i] - i];
            pool.erase(pool.begin() + thread_delete[i] - i);
        }
    }
}

void SimulationEnginePartialMultiThread::get_observations(EngineDataContainer *dc, Organism *&organism,
                                                          std::vector<std::vector<Observation>> &organism_observations,
                                                          SimulationParameters *sp, int organism_num) {
    if (organism->organism_anatomy->_eye_blocks <= 0 || organism->organism_anatomy->_mover_blocks <=0) {return;}
    if (organism->move_counter != 0) {return;}
    auto eye_i = -1;
    for (auto & block: organism->organism_anatomy->_organism_blocks) {
        if (block.type != BlockTypes::EyeBlock) {continue;}
        eye_i++;
        auto pos_x = organism->x + block.get_pos(organism->rotation).x;
        auto pos_y = organism->y + block.get_pos(organism->rotation).y;
        // getting global rotation on a simulation grid
        auto block_rotation = block.get_block_rotation_on_grid(organism->rotation);

        auto offset_x = 0;
        auto offset_y = 0;

        switch (block_rotation) {
            case Rotation::UP:
                offset_y = -1;
                break;
            case Rotation::LEFT:
                offset_x = -1;
                break;
            case Rotation::DOWN:
                offset_y = 1;
                break;
            case Rotation::RIGHT:
                offset_x = 1;
                break;
        }

        auto last_observation = Observation{EmptyBlock, 0, block.rotation};

        for (int i = 1; i < sp->look_range; i++) {
            pos_x += offset_x;
            pos_y += offset_y;

            if (SimulationEngineSingleThread::check_if_out_of_bounds(dc, pos_x, pos_y)) {break;}

            last_observation.type = dc->CPU_simulation_grid[pos_x][pos_y].type;
            last_observation.distance = i;

            if (last_observation.type == BlockTypes::WallBlock) {break;}
            if (last_observation.type == BlockTypes::FoodBlock) {break;}
            if (last_observation.type != BlockTypes::EmptyBlock) {
                if (!sp->organism_self_blocks_block_sight) {
                    if(dc->CPU_simulation_grid[pos_x][pos_y].organism == organism) {
                        continue;
                    }
                }
                break;
            }
        }
        organism_observations[organism_num][eye_i] = last_observation;
    }
}

//TODO only 58% of thread's time it is working, other 42% it sits waiting.

void SimulationEnginePartialMultiThread::thread_tick(PartialSimulationStage stage, EngineDataContainer *dc,
                                                     SimulationParameters *sp, lehmer64 *gen, int thread_num) {
    switch (stage) {
        case PartialSimulationStage::PlaceOrganisms:
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                place_organism(dc, dc->to_place_organisms[i]);
            }
            break;
        case PartialSimulationStage::ProduceFood:
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                SimulationEngineSingleThread::produce_food(dc, sp, dc->organisms_pools[thread_num][i], *gen);
            }
            break;
        case PartialSimulationStage::EatFood:
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                SimulationEnginePartialMultiThread::eat_food(dc, sp, dc->organisms_pools[thread_num][i]);
            }
            break;
        case PartialSimulationStage::ApplyDamage:
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                SimulationEngineSingleThread::apply_damage(dc, sp, dc->organisms_pools[thread_num][i]);
            }
            break;
        case PartialSimulationStage::TickLifetime:
            dc->threaded_to_erase[thread_num].clear();
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                SimulationEnginePartialMultiThread::tick_lifetime(dc, dc->threaded_to_erase, dc->organisms_pools[thread_num][i], thread_num, i);
            }
            break;
        case PartialSimulationStage::GetObservations:
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                SimulationEnginePartialMultiThread::get_observations(dc, dc->organisms_pools[thread_num][i],
                                                                     dc->pooled_organisms_observations[thread_num], sp,
                                                                     i);
            }
            break;
        case PartialSimulationStage::ThinkDecision:
            for (int i = 0; i < dc->organisms_pools[thread_num].size(); i++) {
                dc->organisms_pools[thread_num][i]->think_decision(dc->pooled_organisms_observations[thread_num][i], gen);
            }
            break;
    }
}