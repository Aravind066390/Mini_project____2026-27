program benchmark
    implicit none

    integer :: start_count, end_count, count_rate
    real(8) :: elapsed

    call system_clock(start_count, count_rate)

    call execute_command_line("./test")

    call system_clock(end_count)

    elapsed = real(end_count - start_count, 8) / real(count_rate, 8)

    print *
    print *, "-----------------------------"
    print *, "Test execution time:"
    print *, elapsed, " seconds"
    print *, "-----------------------------"

end program benchmark
