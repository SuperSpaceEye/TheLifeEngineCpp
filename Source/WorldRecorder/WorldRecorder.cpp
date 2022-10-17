//
// Created by spaceeye on 02.10.22.
//

#include "WorldRecorder.h"
#include "../Stuff/DataSavingFunctions.h"

#include <utility>

const int BUFFER_VERSION = 1;

void TransactionBuffer::start_recording(std::string path_to_save_, int width_, int height_, int buffer_size_) {
    path_to_save = std::move(path_to_save_);
    buffer_size = buffer_size_;
    width = width_;
    height = height_;
    buffer_pos = 0;
    saved_buffers = 1;
    recorded_transactions = 0;

    transactions.reserve(buffer_size);
    transactions.emplace_back();
    transactions[0].starting_point = true;
}

void TransactionBuffer::record_food_change(int x, int y, bool added) {
    transactions[buffer_pos].food_change.emplace_back(x, y, added);
}

void TransactionBuffer::record_new_organism(Organism &organism) {
    auto new_organism = Organism(&organism);
    new_organism.vector_index = organism.vector_index;
    transactions[buffer_pos].organism_change.emplace_back(std::move(new_organism));
}

void TransactionBuffer::record_organism_dying(int organism_index) {
    transactions[buffer_pos].dead_organisms.emplace_back(organism_index);
}

void TransactionBuffer::record_organism_move_change(int vector_index, int x, int y, Rotation rotation) {
    transactions[buffer_pos].move_change.emplace_back(vector_index, rotation, x, y);
}

void TransactionBuffer::record_recenter_to_imaginary_pos(bool state) {
    transactions[buffer_pos].recenter_to_imaginary_pos = state;
}

void TransactionBuffer::record_wall_changes(int x, int y, bool added) {
    transactions[buffer_pos].wall_change.emplace_back(x, y, added);
}

void TransactionBuffer::record_reset() {
    transactions[buffer_pos].reset = true;
}

void TransactionBuffer::record_compressed(int pos1, int pos2) {
    transactions[buffer_pos].compressed_change.emplace_back(std::pair<int, int>{pos1, pos2});
}

void TransactionBuffer::record_transaction() {
    if (transactions.size() < buffer_size) {
        transactions.emplace_back();
        buffer_pos++;
        recorded_transactions++;
        transactions[buffer_pos].starting_point = false;
        return;
    }

    flush_transactions();
}

void TransactionBuffer::flush_transactions() {
    if (buffer_pos == 0) { return;}

    auto path = path_to_save + "/" + std::to_string(saved_buffers);
    std::ofstream out(path, std::ios::out | std::ios::binary);
    out.write((char*)&BUFFER_VERSION, sizeof(int));
    out.write((char*)&width, sizeof(int));
    out.write((char*)&height, sizeof(int));
    out.write((char*)&buffer_pos, sizeof(int));

    for (auto & transaction: transactions) {
        out.write((char*)&transaction.starting_point, sizeof(bool));
        out.write((char*)&transaction.reset, sizeof(bool));
        out.write((char*)&transaction.recenter_to_imaginary_pos, sizeof(bool));
        out.write((char*)&transaction.uses_occ, sizeof(bool));

        int size = transaction.organism_change.size();
        out.write((char*)&size, sizeof(int));
        for (auto & o: transaction.organism_change) {
            DataSavingFunctions::write_organism(out, &o);
            out.write((char*)&o.vector_index, sizeof(int));
        }

        size = transaction.food_change.size();
        out.write((char*)&size, sizeof(int));
        out.write((char*)transaction.food_change.data(), sizeof(FoodChange)*size);

        size = transaction.dead_organisms.size();
        out.write((char*)&size, sizeof(int));
        out.write((char*)transaction.dead_organisms.data(), sizeof(int)*size);

        size = transaction.move_change.size();
        out.write((char*)&size, sizeof(int));
        out.write((char*)transaction.move_change.data(), sizeof(MoveChange)*size);

        size = transaction.wall_change.size();
        out.write((char*)&size, sizeof(int));
        out.write((char*)transaction.wall_change.data(), sizeof(WallChange)*size);

        size = transaction.compressed_change.size();
        out.write((char*)&size, sizeof(int));
        out.write((char*)transaction.compressed_change.data(), sizeof(std::pair<int, int>)*size);
    }

    out.close();

    saved_buffers++;
    transactions.clear();
    transactions.emplace_back();
    buffer_pos = 0;
}

void TransactionBuffer::finish_recording() {
    flush_transactions();

    width = 0;
    height = 0;
    buffer_pos = 0;
    saved_buffers = 1;
    buffer_size = 0;
    path_to_save = "";
    recorded_transactions = 0;
    transactions = std::vector<Transaction>();
}

void TransactionBuffer::resize_buffer(int new_buffer_size) {
    if (new_buffer_size == buffer_size) { return;}
    if (new_buffer_size > buffer_size) {
        transactions.reserve(new_buffer_size);
    } else if (new_buffer_size < buffer_size) {
        flush_transactions();
        transactions = std::vector<Transaction>();
        transactions.reserve(new_buffer_size);
        transactions.emplace_back();
    }
    buffer_size = new_buffer_size;
}

bool TransactionBuffer::load_buffer_metadata(std::string &path_to_buffer, int &width, int &height, int &piece_len) {
    std::ifstream in(path_to_buffer, std::ios::in | std::ios::binary);

    int version;
    in.read((char*)&version, sizeof(int));
    if (version != BUFFER_VERSION) {in.close(); return false;}

    in.read((char*)&width, sizeof(int));
    in.read((char*)&height, sizeof(int));
    in.read((char*)&piece_len, sizeof(int));

    return true;
}

bool TransactionBuffer::load_buffer(std::string & path_to_buffer) {
    std::ifstream in(path_to_buffer, std::ios::in | std::ios::binary);
    int version;
    in.read((char*)&version, sizeof(int));
    if (version != BUFFER_VERSION) {return false;}

    in.read((char*)&width, sizeof(int));
    in.read((char*)&height, sizeof(int));
    in.read((char*)&buffer_pos, sizeof(int));

    SimulationParameters sp;
    OrganismBlockParameters bp;
    OCCParameters occp;
    OCCLogicContainer occl;

    transactions.resize(buffer_pos+1);

    for (auto & transaction: transactions) {
        in.read((char*)&transaction.starting_point, sizeof(bool));
        in.read((char*)&transaction.reset, sizeof(bool));
        in.read((char*)&transaction.recenter_to_imaginary_pos, sizeof(bool));
        in.read((char*)&transaction.uses_occ, sizeof(bool));
        sp.use_occ = transaction.uses_occ;
        sp.recenter_to_imaginary_pos = transaction.recenter_to_imaginary_pos;

        int size;
        in.read((char*)&size, sizeof(int));
        transaction.organism_change.resize(size);
        for (auto & o: transaction.organism_change) {
            DataSavingFunctions::read_organism(in, sp, bp, &o, occp, occl);
            in.read((char*)&o.vector_index, sizeof(int));
        }

        in.read((char*)&size, sizeof(int));
        transaction.food_change.resize(size);
        in.read((char*)transaction.food_change.data(), sizeof(FoodChange)*size);

        in.read((char*)&size, sizeof(int));
        transaction.dead_organisms.resize(size);
        in.read((char*)transaction.dead_organisms.data(), sizeof(int)*size);

        in.read((char*)&size, sizeof(int));
        transaction.move_change.resize(size);
        in.read((char*)transaction.move_change.data(), sizeof(MoveChange)*size);

        in.read((char*)&size, sizeof(int));
        transaction.wall_change.resize(size);
        in.read((char*)transaction.wall_change.data(), sizeof(WallChange)*size);

        in.read((char*)&size, sizeof(int));
        transaction.compressed_change.resize(size);
        in.read((char*)transaction.compressed_change.data(), sizeof(std::pair<int, int>)*size);
    }

    in.close();

    return true;
}
