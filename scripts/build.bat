@echo off
rem MatrixDSL - build everything and run the test suite (Windows).
cmake -S "%~dp0.." -B "%~dp0../build" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build "%~dp0../build" -j || exit /b 1
ctest --test-dir "%~dp0../build" --output-on-failure
