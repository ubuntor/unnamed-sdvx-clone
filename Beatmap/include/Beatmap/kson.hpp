#pragma once

template<typename T>
struct ByPulse {
	uint32 tick;
	T value;
};

template<typename T>
struct ByMeasure {
	uint32 idx;
	T value;
};

struct TimeSig {
	uint32 n;
	uint32 d;
};

struct GraphPoint {
	uint32 tick;
	double value;
	double value_final;
	double a;
	double b;
};

struct Interval {
	uint32 tick;
	uint32 length;
};