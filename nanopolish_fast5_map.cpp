//---------------------------------------------------------
// Copyright 2015 Ontario Institute for Cancer Research
// Written by Jared Simpson (jared.simpson@oicr.on.ca)
//---------------------------------------------------------
//
// nanopolish_fast5_map - a simple map from a read
// name to a fast5 file path
#include <stdio.h>
#include <inttypes.h>
#include <zlib.h>
#include <fstream>
#include <ostream>
#include <iostream>
#include <sys/stat.h>
#include "nanopolish_fast5_map.h"
#include "htslib/htslib/kseq.h"

//
#define FOFN_SUFFIX ".fast5.fofn"

KSEQ_INIT(gzFile, gzread);

Fast5Map::Fast5Map(const std::string& fasta_filename)
{
    // If the fofn file exists, load from it
    // otherwise parse the entire fasta file
    std::string fofn_filename = fasta_filename + FOFN_SUFFIX;
    struct stat file_s;
    int ret = stat(fofn_filename.c_str(), &file_s);
    if(ret == 0) {
        load_from_fofn(fofn_filename);
    } else {
        load_from_fasta(fasta_filename);
    }
}

std::string Fast5Map::get_path(const std::string& read_name) const
{
    std::map<std::string, std::string>::const_iterator 
        iter = read_to_path_map.find(read_name);

    if(iter == read_to_path_map.end()) {
        fprintf(stderr, "error: could not find fast5 path for %s\n", read_name.c_str());
        exit(EXIT_FAILURE);
    }

    return iter->second;
}

//
void Fast5Map::load_from_fasta(std::string fasta_filename)
{
    printf("Loading from FASTA %s\n", fasta_filename.c_str());

    gzFile gz_fp;

    FILE* fp = fopen(fasta_filename.c_str(), "r");
    if(fp == NULL) {
        fprintf(stderr, "error: could not open %s for read\n", fasta_filename.c_str());
        exit(EXIT_FAILURE);
    }

    gz_fp = gzdopen(fileno(fp), "r");
    if(gz_fp == NULL) {
        fprintf(stderr, "error: could not open %s using gzdopen\n", fasta_filename.c_str());
        exit(EXIT_FAILURE);
    }

    kseq_t* seq = kseq_init(gz_fp);
    
    while(kseq_read(seq) >= 0) {
        if(seq->comment.l == 0) {
            fprintf(stderr, "error: no path associated with read %s\n", seq->name.s);
            exit(EXIT_FAILURE);
        }
        read_to_path_map[seq->name.s] = seq->comment.s;
    }
    kseq_destroy(seq);
    gzclose(gz_fp);
    fclose(fp);

    // Write the map as a fofn file so next time we don't have to parse
    // the entire fasta
    write_to_fofn(fasta_filename + FOFN_SUFFIX);
}

void Fast5Map::write_to_fofn(std::string fofn_filename)
{
    std::ofstream outfile(fofn_filename.c_str());

    for(std::map<std::string, std::string>::iterator iter = read_to_path_map.begin();
        iter != read_to_path_map.end(); ++iter) {
        outfile << iter->first << "\t" << iter->second << "\n";
    }
}

//
void Fast5Map::load_from_fofn(std::string fofn_filename)
{
    printf("Loading from FOFN %s\n", fofn_filename.c_str());
    std::ifstream infile(fofn_filename.c_str());

    if(infile.bad()) {
        fprintf(stderr, "error: could not read fofn %s\n", fofn_filename.c_str());
        exit(EXIT_FAILURE);
    }

    std::string name;
    std::string path;
    while(infile >> name >> path) {
        read_to_path_map[name] = path;
    }
}
