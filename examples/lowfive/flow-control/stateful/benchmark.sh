if [[ $1 == "-h" ]]; then
    echo "Usage: ./benchmark.sh [OPTIONS]"
    echo "Options:"
    echo "  -h: Display this help message"
    exit 0
fi

python benchmark.py benchmark.yaml