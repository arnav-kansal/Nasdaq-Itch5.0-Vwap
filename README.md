# VWAP - NASDAQ ITCH 5.0

Calculate the Volume weighted average price of each stock at all trading hours given the NASDAQ ITCH 5.0 tick data file.

## Getting Started

### Prerequisites

```
g++ with c++11 std support
boost::program_options, iostreams
```

### Building

```
g++ -std=c++11 -O3 -lboost_iostreams -lboost_program_options Vwap.C
```

## Running 

```
mkdir <output-dir>
./a.out -i <tick-file> -o <output-dir>
```

##Example

```
mkdir outputs
./a.out -i 12292017.NASDAQ_ITCH50 -o outputs/
```
