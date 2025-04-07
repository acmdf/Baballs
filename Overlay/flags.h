#define FLAG_ROUTINE_1          (1U << 0)  
#define FLAG_ROUTINE_2          (1U << 1)
#define FLAG_ROUTINE_3          (1U << 2) 
#define FLAG_ROUTINE_4          (1U << 3) 
#define FLAG_ROUTINE_5          (1U << 4) 
#define FLAG_ROUTINE_6          (1U << 5)
#define FLAG_ROUTINE_7          (1U << 6)
#define FLAG_ROUTINE_8          (1U << 7) 
#define FLAG_ROUTINE_9          (1U << 8) 
#define FLAG_ROUTINE_10         (1U << 9) 
#define FLAG_ROUTINE_11         (1U << 10) 
#define FLAG_ROUTINE_12         (1U << 11)
#define FLAG_ROUTINE_13         (1U << 12) 
#define FLAG_ROUTINE_14         (1U << 13) 
#define FLAG_ROUTINE_15         (1U << 14) 
#define FLAG_ROUTINE_16         (1U << 15) 
#define FLAG_ROUTINE_17         (1U << 16) 
#define FLAG_ROUTINE_18         (1U << 17) 
#define FLAG_ROUTINE_19         (1U << 18) 
#define FLAG_ROUTINE_20         (1U << 19) 
#define FLAG_ROUTINE_21         (1U << 20)
#define FLAG_ROUTINE_22         (1U << 21) 
#define FLAG_ROUTINE_23         (1U << 22) 
#define FLAG_ROUTINE_24         (1U << 23)  

#define FLAG_CONVERGENCE        (1U << 24)  
#define FLAG_IN_MOVEMENT        (1U << 25)  
#define FLAG_RESTING            (1U << 26) 


#define FLAG_ROUTINE_COMPLETE   (1U << 30)


/*

the step flags are for the various stages of calibration

convergence is for the convergence step

"in movement" denotes if a sample is from when the tracking dot is in movement
"resting" denotes when the tracking dot has been resting for long enough that the user should be comfortably focused on it

*/
