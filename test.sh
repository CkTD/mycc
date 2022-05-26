passed=0
failed=0

function failed {
    file=$1
    msg=$2
    prog=$(cat $file | sed -z 's/\s\s*/ /g')
    ((failed++))
    echo -ne "\r\033[K=== $file FAILED ===\n"
    echo "    $msg"
    echo "    $prog"
}

function test {
    file=$1
    prog="$(cat $file)"

    # expected output
    gcc -std=c99 -pedantic -Werror -Wno-implicit-function-declaration -Wno-builtin-declaration-mismatch -o temp.out $file 2>/dev/null
    if [[ $? -ne 0 ]]; then
        failed "$file" "can't compile(gcc)"
        return
    fi

    expected="$(./temp.out)"
    if [[ $? -ne 0 ]]; then
        failed "$file" "non-zero exit code(gcc)"
        return
    fi

    # compile input
    ./mycc $file -o temp.s 2>/dev/null
    if [[ $? -ne 0 ]]; then
        failed "$file" "can't compile"
        return
    fi

    # build executable
    gcc temp.s -o temp.out 2>/dev/null
    if [[ $? -ne 0 ]]; then
        failed "$file" "can't assembly"
        return
    fi

    # checkout output
    real=$(./temp.out)
    if [[ $? -ne 0 ]]; then
        failed "$file" "non-zero exit code"
        return
    fi
    if [[ "${real}" != "${expected}" ]]; then
        failed "$file" "bad output"
        return
    fi

    ((passed++))
}

for file in $(ls test/*.c | sort -n); do
    test $file
    echo -ne "\r\033[KTest Summary: ${passed} PASSED, ${failed} FAILED"
done
echo -ne "\n"
