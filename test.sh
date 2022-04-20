TF=($(ls test/*.in | sort -n))
EF=($(ls test/*.out | sort -n))

T=()
E=()

for i in ${!TF[@]}; do
    T+=("$(cat ${TF[$i]})")
    E+=("$(cat ${EF[$i]})")
done

passed=0
failed=0

for i in ${!T[@]}; do
    prog=${T[$i]}
    expected=${E[$i]}

    # compile input
    ./mycc "${prog}" >temp.S 2>/dev/null
    if [ $? != "0" ]; then
        failed=$((failed + 1))
        echo "=== ${TF[$i]} FAILED (compile) ==="
        echo prog ${prog}
        continue
    fi

    # build executable
    gcc temp.S -o temp.out 2>/dev/null
    if [ $? != "0" ]; then
        failed=$((failed + 1))
        echo "=== ${TF[$i]} FAILED (assembly) ==="
        echo prog ${prog}
        continue
    fi

    # checkout output
    real=$(./temp.out)
    if [ "${real}" != "${expected}" ]; then
        failed=$((failed + 1))
        echo "=== ${TF[$i]} FAILED (bad output) ==="
        echo prog ${prog}
        echo got ${real}
        echo expected ${expected}
        continue
    fi

    passed=$((passed + 1))
done

echo ${passed} PASSED, ${failed} FAILED
