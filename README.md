# timetabling

A timetabling problem is a setting with four parameters:

  - a finite set of time-slots
  - a finite set of resources
  - a finite set of meetings
  - a finite set of constraints
  
The aim is to assign time-slots and resources to the meetings so as
to satisfy the constraints as much as possible.

# The problem

Let us consider a set E of exams, to be scheduled during an examination period at the
end of the semester, and a set S of students. Each student is enrolled in a subset of
exams, at least one. The examination period is divided in tmax ordered time-slots.

By law, conflicting exams (i.e. having enrolled students in common) cannot take place
during the same time-slot. Moreover, to incentive universities at creating timetables
more sustainable for the students, the Ministry of Education assigns a penalty for each
couple of conflicting exams scheduled up to 5 time-slots apart. More precisely, given
the number Ne,e' of students enrolled in both conflicting exams e and e', which are
scheduled 'i' time-slots apart, the penalty is calculated as 2^(5-i)Ne,e'/|S|.

The Examination Timetabling Problem (ETP) aims at assigning each exam to a
specific time-slot ensuring that:

  - each exam is scheduled once during the period,
  - two conflting exams are not scheduled in the same time-slot.
  
The objective is to minimize the total penalty resulting from the created timetable.

Assumptions:

  - during each time-slot there is always a number of available rooms greater than the total number of exams;
  - rooms have enough capacity with respect to the number of enrolled students.
  
  # Instances format
  
  - instanceXX.exm: defines the total number of students enrolled per exam.
    Format: 1 line per exam. Each line has the format 
    
          INT1 INT2
          
    where INT1 is the exam ID and INT2 is the number of enrolled students in INT1.
    
   - instanceXX.slo: defines the length of the examination period.
     Format: a single value in the format 
     
          INT
          
     where INT is the number of available time-slots tmax. Hence, time-slots will
     have numeric IDs {1, 2, ..., tmax}.
     
   - instanceXX.stu: defines the exams in which each student is enrolled.
     Format: 1 line for each enrollment. Each line has the format

          sINT1 INT2
          
     where INT1 is the student ID and INT2 is the ID of the exam in which student
     INT1 is enrolled.
