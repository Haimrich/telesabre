// Benchmark was created by MQT Bench on 2024-03-17
// For more information about MQT Bench, please visit https://www.cda.cit.tum.de/mqtbench/
// MQT Bench version: 1.1.0
// Qiskit version: 1.0.2
// Used Gate Set: ['id', 'rz', 'sx', 'x', 'cx', 'measure', 'barrier']

OPENQASM 2.0;
include "qelib1.inc";
qreg q[64];
creg meas[64];
rz(pi/2) q[0];
sx q[0];
rz(pi/2) q[0];
cx q[0],q[1];
rz(pi/2) q[1];
sx q[1];
rz(pi/2) q[1];
rz(pi/2) q[2];
sx q[2];
rz(pi/2) q[2];
cx q[2],q[3];
rz(pi/2) q[3];
sx q[3];
rz(pi/2) q[3];
rz(pi/2) q[4];
sx q[4];
rz(pi/2) q[4];
cx q[4],q[5];
rz(pi/2) q[5];
sx q[5];
rz(pi/2) q[5];
cx q[1],q[6];
rz(pi/2) q[6];
sx q[6];
rz(pi/2) q[6];
cx q[6],q[7];
rz(pi/2) q[7];
sx q[7];
rz(pi/2) q[7];
rz(pi/2) q[8];
sx q[8];
rz(pi/2) q[8];
cx q[8],q[9];
rz(pi/2) q[9];
sx q[9];
rz(pi/2) q[9];
rz(pi/2) q[10];
sx q[10];
rz(pi/2) q[10];
cx q[10],q[11];
rz(pi/2) q[11];
sx q[11];
rz(pi/2) q[11];
rz(pi/2) q[12];
sx q[12];
rz(pi/2) q[12];
cx q[4],q[13];
cx q[12],q[13];
rz(pi/2) q[13];
sx q[13];
rz(pi/2) q[13];
rz(pi/2) q[14];
sx q[14];
rz(pi/2) q[14];
cx q[14],q[15];
rz(pi/2) q[15];
sx q[15];
rz(pi/2) q[15];
rz(pi/2) q[16];
sx q[16];
rz(pi/2) q[16];
cx q[8],q[17];
cx q[16],q[17];
rz(pi/2) q[17];
sx q[17];
rz(pi/2) q[17];
rz(pi/2) q[18];
sx q[18];
rz(pi/2) q[18];
cx q[18],q[19];
rz(pi/2) q[19];
sx q[19];
rz(pi/2) q[19];
cx q[19],q[20];
rz(pi/2) q[20];
sx q[20];
rz(pi/2) q[20];
rz(pi/2) q[21];
sx q[21];
rz(pi/2) q[21];
cx q[21],q[22];
rz(pi/2) q[22];
sx q[22];
rz(pi/2) q[22];
rz(pi/2) q[23];
sx q[23];
rz(pi/2) q[23];
cx q[23],q[24];
rz(pi/2) q[24];
sx q[24];
rz(pi/2) q[24];
cx q[20],q[25];
cx q[23],q[25];
rz(pi/2) q[25];
sx q[25];
rz(pi/2) q[25];
rz(pi/2) q[26];
sx q[26];
rz(pi/2) q[26];
cx q[26],q[27];
rz(pi/2) q[27];
sx q[27];
rz(pi/2) q[27];
rz(pi/2) q[28];
sx q[28];
rz(pi/2) q[28];
cx q[7],q[29];
cx q[28],q[29];
rz(pi/2) q[29];
sx q[29];
rz(pi/2) q[29];
cx q[5],q[30];
rz(pi/2) q[30];
sx q[30];
rz(pi/2) q[30];
cx q[0],q[31];
cx q[9],q[31];
rz(pi/2) q[31];
sx q[31];
rz(pi/2) q[31];
rz(pi/2) q[32];
sx q[32];
rz(pi/2) q[32];
cx q[32],q[33];
rz(pi/2) q[33];
sx q[33];
rz(pi/2) q[33];
rz(pi/2) q[34];
sx q[34];
rz(pi/2) q[34];
cx q[34],q[35];
rz(pi/2) q[35];
sx q[35];
rz(pi/2) q[35];
rz(pi/2) q[36];
sx q[36];
rz(pi/2) q[36];
cx q[36],q[37];
rz(pi/2) q[37];
sx q[37];
rz(pi/2) q[37];
cx q[3],q[38];
rz(pi/2) q[38];
sx q[38];
rz(pi/2) q[38];
cx q[38],q[39];
rz(pi/2) q[39];
sx q[39];
rz(pi/2) q[39];
cx q[26],q[40];
rz(pi/2) q[40];
sx q[40];
rz(pi/2) q[40];
cx q[16],q[41];
rz(pi/2) q[41];
sx q[41];
rz(pi/2) q[41];
cx q[41],q[42];
rz(pi/2) q[42];
sx q[42];
rz(pi/2) q[42];
cx q[11],q[43];
cx q[33],q[43];
rz(pi/2) q[43];
sx q[43];
rz(pi/2) q[43];
cx q[21],q[44];
cx q[39],q[44];
rz(pi/2) q[44];
sx q[44];
rz(pi/2) q[44];
cx q[28],q[45];
rz(pi/2) q[45];
sx q[45];
rz(pi/2) q[45];
cx q[27],q[46];
cx q[37],q[46];
rz(pi/2) q[46];
sx q[46];
rz(pi/2) q[46];
cx q[2],q[47];
rz(pi/2) q[47];
sx q[47];
rz(pi/2) q[47];
cx q[10],q[48];
cx q[45],q[48];
rz(pi/2) q[48];
sx q[48];
rz(pi/2) q[48];
cx q[22],q[49];
cx q[35],q[49];
rz(pi/2) q[49];
sx q[49];
rz(pi/2) q[49];
cx q[18],q[50];
rz(pi/2) q[50];
sx q[50];
rz(pi/2) q[50];
cx q[50],q[51];
rz(pi/2) q[51];
sx q[51];
rz(pi/2) q[51];
cx q[47],q[52];
rz(pi/2) q[52];
sx q[52];
rz(pi/2) q[52];
cx q[34],q[53];
rz(pi/2) q[53];
sx q[53];
rz(pi/2) q[53];
cx q[52],q[54];
cx q[53],q[54];
rz(pi/2) q[54];
sx q[54];
rz(pi/2) q[54];
cx q[15],q[55];
rz(pi/2) q[55];
sx q[55];
rz(pi/2) q[55];
cx q[14],q[56];
cx q[32],q[56];
rz(pi/2) q[56];
sx q[56];
rz(pi/2) q[56];
cx q[42],q[57];
rz(pi/2) q[57];
sx q[57];
rz(pi/2) q[57];
cx q[55],q[58];
rz(pi/2) q[58];
sx q[58];
rz(pi/2) q[58];
cx q[58],q[59];
rz(pi/2) q[59];
sx q[59];
rz(pi/2) q[59];
cx q[51],q[60];
cx q[59],q[60];
rz(pi/2) q[60];
sx q[60];
rz(pi/2) q[60];
cx q[12],q[61];
cx q[40],q[61];
rz(pi/2) q[61];
sx q[61];
rz(pi/2) q[61];
cx q[24],q[62];
cx q[57],q[62];
rz(pi/2) q[62];
sx q[62];
rz(pi/2) q[62];
cx q[30],q[63];
cx q[36],q[63];
rz(pi/2) q[63];
sx q[63];
rz(pi/2) q[63];