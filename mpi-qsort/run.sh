make; make clean all

./generate 1048576
mpirun -n 1 -machinefile mfile run
./validate
./generate 1048576
mpirun -n 8 -machinefile mfile run
./validate

./generate 4194304
mpirun -n 1 -machinefile mfile run
./validate
./generate 4194304
mpirun -n 8 -machinefile mfile run
./validate

./generate 16777216
mpirun -n 1 -machinefile mfile run
./validate
./generate 16777216
mpirun -n 8 -machinefile mfile run
./validate

./generate 33554432
mpirun -n 1 -machinefile mfile run
./validate
./generate 33554432
mpirun -n 8 -machinefile mfile run
./validate
