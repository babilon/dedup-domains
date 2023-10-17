#!/bin/bash

echo "Clear test artifacts"

# cwd is 'tests/'

/usr/bin/rm -rf ./test_standard_out; /usr/bin/mkdir ./test_standard_out
/usr/bin/rm -rf ./test_standard_sorted; /usr/bin/mkdir ./test_standard_sorted

/usr/bin/rm -rf ./test_pointer_out; /usr/bin/mkdir ./test_pointer_out
/usr/bin/rm -rf ./test_pointer_sorted; /usr/bin/mkdir ./test_pointer_sorted

echo -e "\nRun prune with standard method"
time /usr/bin/python3 ../pfb_dnsbl_prune.py ./001_inputs .fat .pruned --method standard
echo "Compare standard output to benchmark:"
/usr/bin/mv ./001_inputs/*.pruned ./test_standard_out/
/usr/bin/find ./test_standard_out -type f -name '*.pruned' | /usr/bin/rename 's/\.pruned$/.txt/'
/usr/bin/diff -rq ./001_bench_standard/ ./test_standard_out/
ret=$?
if [[ $ret -eq 0 ]]; then
    echo "001 standard: zero diffences"
else
    exit 1
fi

echo -e "\nRun prune with pointer method"
time /usr/bin/python3 ../pfb_dnsbl_prune.py ./001_inputs .fat .pruned --method pointer
echo -e "\nCompare pointer output to benchmark:"
/usr/bin/mv ./001_inputs/*.pruned ./test_pointer_out/
/usr/bin/find -type f -name '*.pruned' | /usr/bin/rename 's/\.pruned$/.txt/'
/usr/bin/diff -rq ./001_bench_pointer/ ./test_pointer_out/
ret=$?
if [[ $ret -eq 0 ]]; then
    echo "001 pointer: zero diffences"
else
    exit 1
fi

echo -e "\nCompare standard output against pointer output:"
for f in ./test_pointer_out/*.txt; do
    base=$(basename -s .txt ${f})
    sort ${f} > ./test_pointer_sorted/${base}.sorted
done

for f in ./test_standard_out/*.txt; do
    base=$(basename -s .txt ${f})
    sort ${f} > ./test_standard_sorted/${base}.sorted
done

/usr/bin/diff -rq ./test_pointer_sorted ./test_standard_sorted
ret=$?
if [[ $ret -eq 0 ]]; then
    echo "001 standard vs. pointer: zero diffences"
else
    exit 1
fi
