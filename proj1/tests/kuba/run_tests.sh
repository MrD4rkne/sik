#!/bin/bash

# Define colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
RESET='\033[0m'

if [ $# -ne 2 ]; then
    echo -e "${RED}Usage: $0 <path to peer-time-sync> <path to test-runner>${RESET}"
    exit 1
fi

if [ ! -f $1 ]; then
    echo -e "${RED}File $1 does not exist${RESET}"
    exit 1
fi

if [ ! -x $1 ]; then
    echo -e "${RED}File $1 is not executable${RESET}"
    exit 1
fi

peer_time_sync=$1
test_runner=$2
passed=0
failed=0
i=0

echo -e "${BOLD}======= Test Execution Started $(date) =======${RESET}\n"
echo -e "${BLUE}Using binary: ${BOLD}$peer_time_sync${RESET}\n"

run_test() {
    local testfile=$1
    local test_type=$2
    local tester_out="tester_${i}.out"
    local server_out="server_${i}.out"
    
    echo -e "${YELLOW}[$(date +%H:%M:%S)]${RESET} Running test ${BOLD}#$i${RESET}: ${BLUE}$testfile${RESET} (type: $test_type)"
    
    if [ "$test_type" == "server_first" ]; then
        echo -e "  ${YELLOW}Starting server first...${RESET}"
        "$peer_time_sync" -p 8000 &> "$server_out" &
        SERVER_PID=$!

        sleep 1

        cat $testfile | python3 "$test_runner" &> "$tester_out" &
        TESTER_PID=$!
    else
        echo -e "  ${YELLOW}Starting tester first...${RESET}"

        cat $testfile | python3 "$test_runner" &> "$tester_out" &
        TESTER_PID=$!

        sleep 1

        "$peer_time_sync" -p 8000 -a localhost -r 8001 &> "$server_out" &
        SERVER_PID=$!
    fi

    echo -e "  Process IDs: Server=${SERVER_PID}, Tester=${TESTER_PID}"
    
    success=1
    
    if wait $TESTER_PID; then
        kill $SERVER_PID &> /dev/null
        wait $SERVER_PID
        # if ! diff "$server_out" "$tester_out" > /dev/null; then
        #     echo -e "  ${RED}✗ Test failed - Output mismatch${RESET}"
        #     echo -e "  ${BLUE}Compare files:${RESET} diff $server_out $tester_out"
        #     success=0
        # else
        #     echo -e "  ${GREEN}✓ Test passed${RESET}"
        # fi
    else
        echo -e "  ${RED}✗ Test failed - Tester process exited with error${RESET}"
        kill $SERVER_PID &> /dev/null
        success=0
    fi
    
    if [ $success -eq 1 ]; then
        passed=$((passed + 1))
    else
        failed=$((failed + 1))
    fi
    
    i=$((i + 1))
    echo ""
}

# Process server_first tests
echo -e "${BOLD}===== Running Server First Tests =====${RESET}"
for testfile in ./tests/server_first/*.in; do
    run_test "$testfile" "server_first"
done

# Process server_second tests
echo -e "${BOLD}===== Running Server Second Tests =====${RESET}"
for testfile in ./tests/server_second/*.in; do
    run_test "$testfile" "server_second"
done

# Test summary
echo -e "${BOLD}======= Test Summary =======${RESET}"
echo -e "${GREEN}Passed: $passed${RESET}"
echo -e "${RED}Failed: $failed${RESET}"
echo -e "Total: $((passed + failed)) tests"

if [ $failed -eq 0 ]; then
    echo -e "\n${GREEN}${BOLD}✓ All tests passed successfully!${RESET}"
    exit 0
else
    echo -e "\n${RED}${BOLD}✗ Some tests failed!${RESET}"
    exit 1
fi
