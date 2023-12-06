#ifndef COMAPREWITHBLAST_H
#define COMAPREWITHBLAST_H

#include <unordered_map>
#include <set>
#include <algorithm>
#include <iostream>
#include "BlastReader.h"

using namespace std;

class CompareWithBlast {
public: 

    void determineEditsLocationsAndType(string read, string query, vector<int>& editPos) {
        // Check if the sequences have the same length
        if (read.length() != query.length()) {
            // cout << "q : " << query << endl;
            // cout << "r : " << read << endl;
            cerr << "Error: Sequences have different lengths." << endl;
            return;
        }
        // Iterate through the sequences and compare corresponding characters
        for (size_t i = 0; i < read.length(); ++i) {
            if (read[i] != query[i]) {
                editPos.push_back(i);
            } 
        }
 
    }

    bool checkBlastEditPositions(BlastReader::Blast& blastAlignment, Config cfg)
    {
        vector<int> editPos;
        determineEditsLocationsAndType(blastAlignment.readAligned, blastAlignment.queryAligned, editPos);
        // if (editPos.size() != blastAlignment.Mismatches) 
        // {
        //     cout << "Error: Edit distance not equals mismatch." << endl;
        //     return false;
        // }
        bool firstKmerInFrontRegionDismissed = false, lastKmerInFrontRegionDismissed = false,
            firstKmerInBackRegionDismissed = false, lastKmerInBacktRegionDismissed = false;
        bool frontRegionDismissed = false, backRegionDismissed = false;
        uint32_t queryS = blastAlignment.queryS - 1;
        for(auto v : editPos)
        {
            if(((v + queryS) >= 0 && (v + queryS) <= cfg.minExactMatchLen))
            {
                firstKmerInFrontRegionDismissed = true;
            }
            if(((v + queryS) >= (cfg.regionSize - cfg.minExactMatchLen) && (v + queryS) <= cfg.regionSize))
            {
                lastKmerInFrontRegionDismissed = true;
            }
            if(((v + queryS) >= (cfg.contigSize - cfg.regionSize)) && ((v + queryS) <= (cfg.contigSize - cfg.regionSize + cfg.minExactMatchLen)))
            {
                firstKmerInBackRegionDismissed = true;
            }
            if(((v + queryS) >= (cfg.contigSize - cfg.minExactMatchLen)) && ((v + queryS) <= cfg.contigSize))
            {
                lastKmerInBacktRegionDismissed = true;
            }
        }
        if(firstKmerInFrontRegionDismissed && lastKmerInFrontRegionDismissed)
            frontRegionDismissed = true;
        if(firstKmerInBackRegionDismissed && lastKmerInBacktRegionDismissed)
            backRegionDismissed = true;
        if(queryS > (cfg.editDistance - 1))
            frontRegionDismissed = true;
        if(queryS + blastAlignment.AlignmentLength <= cfg.contigSize - cfg.editDistance) //query start position + alignment len should be larger than conig size - allowed edit
            backRegionDismissed = true;
        if(frontRegionDismissed && backRegionDismissed)
            return false;
        if(queryS > (cfg.contigSize - cfg.regionSize - 1 + cfg.editDistance)) //first bp of back region starts from 99
            return false;
        return true;
    }

    void comparePmWithBlast(const Config& cfg, tsl::robin_map <uint32_t, string>& reads, tsl::robin_map <uint32_t, string>& queries, string comparisonResultsFileAddress, IndexContainer<uint32_t, LevAlign>& pmAlignments, vector<std::pair<uint32_t, uint32_t>>& alnPmBLASTHisto, const uint32_t queryCount, string alnPerQueryFileAddress)
    {
        ofstream cmp(comparisonResultsFileAddress);
        ofstream alnPerQ(alnPerQueryFileAddress);
        SamReader sam(cfg.otherToolOutputFileAddress);
        BlastReader blastReader(cfg.otherToolOutputFileAddress);
        IndexContainer<uint32_t, BlastReader::Blast> blastAlignments;
        blastReader.parseFile(queryCount, blastAlignments);
        uint64_t numberOfEqualalignment = 0, numberOfBLASTbetter = 0, numberOfPMbetter = 0, 
        numberOfBLASTbetterExceedMaxEdits = 0, numberOfBLASTbetterWithLowMatchSize = 0,  
        onlyblastFoundMatchForQuery = 0, pmReadIdNotFoundInBLAST = 0, blastReadIdNotFoundInPM = 0, 
        onlyPmFoundMatchForQuery = 0, nonePmblastFoundAlignmentForQuery = 0, numberOfBLASTbetterNotObserveOurCriteria = 0, 
        numberOfQueryContainN = 0, queriesBLASTFoundMatch = 0, pmTotalNumberOfReadIDs = 0, blastTotalNumberOfReadIDs = 0,
        totalNumberOfBlastTruePositive = 0, totalNumberOfBlastFalsePositive = 0;
        set<uint32_t> pmReadPerQuerySet, blastReadPerQuerySet, blastTPReadPerQuerySet;
        uint32_t pmQueriesFound = 0;
        for(uint32_t queryInd = 0; queryInd < queryCount; queryInd++)
        {
            bool blastFound = false, pmFound = false;
            uint32_t blastMatchSize = 0, pmMatchSize = 0;
            string query = queries[queryInd];
            if(query.find('N') != string::npos || query.find('n') != string::npos)
            {
                cmp << "query contains N!" << endl;
                numberOfQueryContainN++; 
                alnPerQ << queryInd << " 0 0"<< endl;
                continue;
            }

            BlastReader::Blast blastAlignment;
            auto blRrange = blastAlignments.getRange(queryInd);
            size_t blastReadPerQuery = distance(blRrange.first, blRrange.second);
            blastTotalNumberOfReadIDs += blastReadPerQuery;
            blastReadPerQuerySet.insert(blastReadPerQuery);
            for (auto it = blRrange.first; it != blRrange.second; it++) 
            {
                BlastReader::Blast aln = it->second;
                string blastR = reads[aln.readId]; 
                if(blastR.find('N') != string::npos || blastR.find('n') != string::npos)
                    continue;
                if (aln.Mismatches + aln.InDels <= cfg.editDistance)
                {
                    blastFound = true;
                    if (aln.AlignmentLength  > blastAlignment.AlignmentLength)
                    {
                        blastAlignment = aln;
                    } else if (aln.AlignmentLength == blastAlignment.AlignmentLength)
                    {
                        if (aln.Mismatches + aln.InDels < blastAlignment.Mismatches + blastAlignment.InDels)
                        {
                            blastAlignment = aln;
                        }
                    }
                } 
            }
            string blastRead = reads[blastAlignment.readId];
            auto pmRange = pmAlignments.getRange(queryInd);
            size_t pmReadPerQuery = distance(pmRange.first, pmRange.second);
            pmTotalNumberOfReadIDs += pmReadPerQuery;
            pmReadPerQuerySet.insert(pmReadPerQuery);
            LevAlign pmAlignment;
            for (auto it = pmRange.first; it != pmRange.second; it++) 
            {
                pmFound = true;
                LevAlign aln = it->second;
                if (aln.numberOfMatches + aln.numberOfInDel > pmAlignment.numberOfMatches + pmAlignment.numberOfInDel)
                {
                    pmAlignment = aln;
                } else if (aln.numberOfMatches + aln.numberOfInDel == pmAlignment.numberOfMatches + pmAlignment.numberOfInDel)
                {
                    if (aln.numberOfInDel + aln.numberOfSub < pmAlignment.numberOfInDel + pmAlignment.numberOfSub) // InDel has the same wight as substitution
                    {
                        pmAlignment = aln;
                    }
                }
            }
            if (pmFound)
            {
                pmQueriesFound++;
                pmMatchSize = sam.countMatches(pmAlignment.cigar);
            }
            if (blastFound)
            {
                queriesBLASTFoundMatch++;
                blastMatchSize = blastAlignment.AlignmentLength;
            }
            alnPmBLASTHisto.push_back(std::make_pair(pmMatchSize, blastMatchSize));
            if (pmFound && !blastFound)
            {
                cmp << "PM outperformed and BLAST did not found" << endl; 
                numberOfPMbetter++;
                onlyPmFoundMatchForQuery++;
                cmp << "<<<<<<<<<<<PM alignment>>>>>>>>>>>" << endl;
                cmp << "Q : " << query << endl;
                cmp << "R : " << reads[pmAlignment.readID] << endl;
                // cmp << "q : " << pmAlignment.alignedQuery << endl;
                // cmp << "r : " << pmAlignment.alignedRead << endl;
                // cmp << "E : " << pmAlignment.editDistanceTypes << endl;
                cmp << "CIGAR : " << pmAlignment.cigar << ", subs : " << pmAlignment.numberOfSub << ", query id : " << queryInd << ", read id : " << pmAlignment.readID << ", flag : " << pmAlignment.flag << ", partial match size : " << pmAlignment.partialMatchSize << ", edits : " << pmAlignment.editDistance << ", R per Q : " << pmReadPerQuery << endl;
                cmp << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
            } else if (!pmFound && blastFound)
            {
                cmp << "BLAST outperformed and PM did not found"; 
                numberOfBLASTbetter++;
                onlyblastFoundMatchForQuery++;
                if (blastAlignment.Mismatches + blastAlignment.InDels > cfg.editDistance)
                {
                    cmp << "with more edits > " << cfg.editDistance << endl;
                    numberOfBLASTbetterExceedMaxEdits++;
                } else if(blastMatchSize < cfg.regionSize) {    
                    cmp << "with low match size : " << blastMatchSize << endl;
                    numberOfBLASTbetterWithLowMatchSize++;
                } else {
                    if (!checkBlastEditPositions(blastAlignment, cfg)){
                        numberOfBLASTbetterNotObserveOurCriteria++;
                        cmp << "the read was supposed to be discarded based on our criteria (edit pos)" << endl;
                    }
                }
                cmp << "<<<<<<<<<<<<<BLAST alignment>>>>>>>>>>>>>" << endl;
                cmp << "Q : " << query << endl;
                cmp << "R : " << blastRead << endl;
                cmp << "AlignmentLength : " << blastAlignment.AlignmentLength << ", subs : " << blastAlignment.Mismatches << ", inDels : " << blastAlignment.InDels << ", query id : " << blastAlignment.queryId << ", read id : " << blastAlignment.readId << ", flag : " << blastAlignment.flag << ", query start pos : " << blastAlignment.queryS << ", read start pos : " << blastAlignment.readS << ", R per Q : " << blastReadPerQuery << endl;
                cmp << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";  
            } else if (pmFound && blastFound) 
            {
                // int bwaInDelSize = bwaSam.countInsertions(blastAlignment.cigar) + bwaSam.countDeletions(blastAlignment.cigar);
                cmp << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";
               
                if (blastMatchSize < pmMatchSize + pmAlignment.numberOfInDel)
                {
                    cmp << "PM outperformed "; 
                    numberOfPMbetter++;
                } else if (blastMatchSize > pmMatchSize + pmAlignment.numberOfInDel)
                {
                    cmp << "BLAST outperformed";
                    numberOfBLASTbetter++;
                    if (blastAlignment.Mismatches + blastAlignment.InDels > cfg.editDistance)
                    {
                        cmp << "with more edits > " << cfg.editDistance << endl;
                        numberOfBLASTbetterExceedMaxEdits++;
                    } else if(blastMatchSize < cfg.regionSize) {
                        cmp << "with low match size : " << blastMatchSize << endl;
                        numberOfBLASTbetterWithLowMatchSize++;
                    } else {
                        if (!checkBlastEditPositions(blastAlignment, cfg))
                        {
                            numberOfBLASTbetterNotObserveOurCriteria++;
                            cmp << "the read was supposed to be discarded based on our criteria (edit pos)" << endl;
                        }
                    }
                } else
                {
                    if (pmAlignment.numberOfSub + pmAlignment.numberOfInDel < blastAlignment.Mismatches + blastAlignment.InDels)
                    {
                        cmp << "PM outperformed with fewer edits" << endl;
                        numberOfPMbetter++;
                    } else if (pmAlignment.numberOfSub + pmAlignment.numberOfInDel > blastAlignment.Mismatches + blastAlignment.InDels)
                    {
                        cmp << "BLAST outperformed";
                        numberOfBLASTbetter++;
                        if (blastAlignment.Mismatches + blastAlignment.InDels > cfg.editDistance)
                        {
                            cmp << "with more edits > " << cfg.editDistance << endl;
                            numberOfBLASTbetterExceedMaxEdits++;
                        } else 
                        {
                            if (!checkBlastEditPositions(blastAlignment, cfg))
                            {
                                numberOfBLASTbetterNotObserveOurCriteria++;
                                cmp << "the read was supposed to be discarded based on our criteria (edit pos)" << endl;
                            } else 
                            {
                                cmp << "with fewer edits" << endl;
                            }  
                        }
                    } else
                    {
                        cmp << "BLAST and PM performed equal" << endl;
                        if (!checkBlastEditPositions(blastAlignment, cfg))
                        {
                            numberOfBLASTbetterNotObserveOurCriteria++;
                            cmp << "the read was supposed to be discarded based on our criteria (edit pos)" << endl;
                        } else 
                        { 
                            numberOfEqualalignment++;
                        }
                    }
                }
                cmp << "<<<<<<<<<<<PM alignment>>>>>>>>>>>" << endl;
                cmp << "Q : " << query << endl;
                cmp << "R : " << reads[pmAlignment.readID] << endl;
                // cmp << "q : " << pmAlignment.alignedQuery << endl;
                // cmp << "r : " << pmAlignment.alignedRead << endl;
                // cmp << "E : " << pmAlignment.editDistanceTypes << endl;
                cmp << "CIGAR : " << pmAlignment.cigar << ", subs : " << pmAlignment.numberOfSub << ", query id : " << queryInd << ", read id : " << pmAlignment.readID << ", flag : " << pmAlignment.flag << ", partial match size : " << pmAlignment.partialMatchSize << ", edits : " << pmAlignment.editDistance << ", R per Q : " << pmReadPerQuery << endl;
                cmp << "<<<<<<<<<<<<<BLAST alignment>>>>>>>>>>>>>" << endl;
                cmp << "Q : " << query << endl;
                cmp << "R : " << blastRead << endl;
                cmp << "AlignmentLength : " << blastAlignment.AlignmentLength << ", subs : " << blastAlignment.Mismatches << ", inDels : " << blastAlignment.InDels << ", query id : " << blastAlignment.queryId << ", read id : " << blastAlignment.readId << ", flag : " << blastAlignment.flag << ", query start pos : " << blastAlignment.queryS << ", read start pos : " << blastAlignment.readS << ", R per Q : " << blastReadPerQuery << endl;
                cmp << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n";  
            } else {
                cmp << "query " << queryInd << " not found in the PM results nor in BLAST" << endl;
                nonePmblastFoundAlignmentForQuery++;
            }
            //check for the read id
            bool found = false;
            for (auto it = pmRange.first; it != pmRange.second; it++) 
            {
                LevAlign aln = it->second;
                found = false;
                for (auto itt = blRrange.first; itt != blRrange.second; itt++) 
                {
                    BlastReader::Blast blast = itt->second;
                    if((uint32_t) aln.readID == blast.readId)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    pmReadIdNotFoundInBLAST++;
                    // cmp << "pmReadIdNotFoundInBLAST = Q :" << blastAlignment.queryId << ", R : " << blastAlignment.readId << endl;
                }
            }
            auto blastTPReadPerQuery = 0;
            for (auto itt = blRrange.first; itt != blRrange.second; itt++) 
            {
                BlastReader::Blast blast = itt->second;
                string blastRead = reads[blast.readId];
                found = false;
                //count True positives
                if (blast.Mismatches + blast.InDels <= cfg.editDistance && blast.AlignmentLength >= cfg.regionSize && checkBlastEditPositions(blast, cfg))
                {
                    blastTPReadPerQuery++;
                    totalNumberOfBlastTruePositive++;
                } else 
                {
                    totalNumberOfBlastFalsePositive++;
                }
                for (auto it = pmRange.first; it != pmRange.second; it++) 
                {
                    LevAlign aln = it->second;
                    if((uint32_t) aln.readID == blast.readId)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    blastReadIdNotFoundInPM++;
                    // cmp << "pmReadIdNotFoundInBLAST = Q :" << blastAlignment.queryId << ", R : " << blastAlignment.readId << endl;
                }
            }
            blastTPReadPerQuerySet.insert(blastTPReadPerQuery);
            //PM and BLAST alignments per query
            alnPerQ << queryInd << " " << pmAlignments.container_.count(queryInd) << " " << blastTPReadPerQuery << endl;
        }
        Utilities<uint32_t> util;   
        tuple<uint32_t, uint32_t, uint32_t> pmReadPerQueryTuple = util.calculateStatistics(pmReadPerQuerySet);
        tuple<uint32_t, uint32_t, uint32_t> blastReadPerQueryTuple = util.calculateStatistics(blastReadPerQuerySet);
        tuple<uint32_t, uint32_t, uint32_t> blastTPReadPerQueryTuple = util.calculateStatistics(blastTPReadPerQuerySet);
        cout << "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<Overall Comparison Results>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>" << endl;
        cmp << left << setw(80) << "# Of queries : " << queryCount << endl;
        cmp << left << setw(80) << "# Of queries that PARMIK found match : " << pmQueriesFound << endl;
        cmp << left << setw(80) << "# Of queries that BLAST found match : " << queriesBLASTFoundMatch << endl;
        cmp << left << setw(80) << "# Of Matched Hits between PM and BLAST : " << numberOfEqualalignment << endl;;
        cmp << left << setw(80) << "# Of readIDs found by PARMIK (total): " << pmTotalNumberOfReadIDs << endl;
        cmp << left << setw(80) << "# Of readIDs found by BLAST (total): " << blastTotalNumberOfReadIDs << endl;
        cmp << left << setw(80) << "# Of  BLAST TP: " << totalNumberOfBlastTruePositive << endl;
        cmp << left << setw(80) << "# Of  BLAST FP: " << totalNumberOfBlastFalsePositive << endl;
        cmp << left << setw(80) << "# Of PM outperformed : " << numberOfPMbetter << endl;
        cmp << left << setw(80) << "# Of BLAST outperformed : " << numberOfBLASTbetter << endl;
        cmp << left << setw(80) << "# Of BLAST outperformed that Exceed Max Edits : " << numberOfBLASTbetterExceedMaxEdits << endl;
        cmp << left << setw(80) << "# Of BLAST outperformed that matchsize < (R - E): " << numberOfBLASTbetterWithLowMatchSize << endl;
        cmp << left << setw(80) << "# Of BLAST outperformed that has edits in minExact-mers of region : " << numberOfBLASTbetterNotObserveOurCriteria << endl;
        cmp << left << setw(80) << "# Of queries that BLAST found matches, not PM : " << onlyblastFoundMatchForQuery << endl;
        cmp << left << setw(80) << "# Of queries that PM found matches, not BLAST : " << onlyPmFoundMatchForQuery << endl;
        cmp << left << setw(80) << "# Of readIDs that PM found but BLAST did not : " << pmReadIdNotFoundInBLAST << endl;
        cmp << left << setw(80) << "# Of readIDs that BLAST found but PM did not : " << blastReadIdNotFoundInPM << endl;
        cmp << left << setw(80) << "# Of queries that neigther BLAST nor PM found any match : " << nonePmblastFoundAlignmentForQuery << endl;
        cmp << left << setw(80) << "# Of queries contains N: " << numberOfQueryContainN << endl;
        cmp << left << setw(80) << "# of Read Per Query found by PM" << "average: " <<  get<0>(pmReadPerQueryTuple) << ", median: " <<  get<1>(pmReadPerQueryTuple) << ", mean: " << get<2>(pmReadPerQueryTuple)<< endl;
        cmp << left << setw(80) << "# of Read Per Query found by BLAST" << "average: " <<  get<0>(blastReadPerQueryTuple) << ", median: " <<  get<1>(blastReadPerQueryTuple) << ", mean: " << get<2>(blastReadPerQueryTuple)<< endl;
        cmp << left << setw(80) << "# of Read TP Per Query found by BLAST" << "average: " <<  get<0>(blastTPReadPerQueryTuple) << ", median: " <<  get<1>(blastTPReadPerQueryTuple) << ", mean: " << get<2>(blastTPReadPerQueryTuple)<< endl;
        cmp.close();
        alnPerQ.close();
    }
};

#endif