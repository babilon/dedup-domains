#!/bin/bash

echo "Clear test artifacts"

# cwd is 'tests/'

\rm -rf ./test_regex_standard_out; \mkdir ./test_regex_standard_out
\rm -rf ./test_regex_standard_sorted; \mkdir ./test_regex_standard_sorted

\rm -rf ./test_regex_pointer_out; \mkdir ./test_regex_pointer_out
\rm -rf ./test_regex_pointer_sorted; \mkdir ./test_regex_pointer_sorted

echo -e "\nRun prune regex with standard method"
\time \python3 ../pfb_dnsbl_prune.py ./001_inputs .fat .pruned --prune-regex --method standard
echo "Compare standard output to benchmark:"
\mv ./001_inputs/*.pruned ./test_regex_standard_out/
\find ./test_regex_standard_out -type f -name '*.pruned' | \rename 's/\.pruned$/.txt/'
\diff -rq ./001_bench_regex_standard/ ./test_regex_standard_out/
ret=$?
if [[ $ret -eq 0 ]]; then
    echo "001 standard: zero diffences"
else
    exit 1
fi

echo -e "\nRun prune regex with pointer method"
\time \python3 ../pfb_dnsbl_prune.py ./001_inputs .fat .pruned --prune-regex --method pointer
echo -e "\nCompare pointer output to benchmark:"
\mv ./001_inputs/*.pruned ./test_regex_pointer_out/
\find -type f -name '*.pruned' | \rename 's/\.pruned$/.txt/'
\diff -rq ./001_bench_regex_pointer/ ./test_regex_pointer_out/
ret=$?
if [[ $ret -eq 0 ]]; then
    echo "001 pointer: zero diffences"
else
    exit 1
fi

echo -e "\nCompare standard output against pointer output:"
for f in ./test_regex_pointer_out/*.txt; do
    base=$(basename -s .txt ${f})
    sort ${f} > ./test_regex_pointer_sorted/${base}.sorted
done

for f in ./test_regex_standard_out/*.txt; do
    base=$(basename -s .txt ${f})
    sort ${f} > ./test_regex_standard_sorted/${base}.sorted
done

\diff -rq ./test_regex_pointer_sorted ./test_regex_standard_sorted
ret=$?
if [[ $ret -eq 0 ]]; then
    echo "001 standard vs. pointer: zero diffences"
else
    exit 1
fi
