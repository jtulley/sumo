//*!!Sensor,    S1,                  us1, sensorSONAR,      ,                    !!*//
//*!!Sensor,    S2,                  us2, sensorSONAR,      ,                    !!*//
//*!!Sensor,    S3,                  us3, sensorSONAR,      ,                    !!*//
//*!!Motor,  motorA,              right_m, tmotorNxtEncoderClosedLoop,           !!*//
//*!!Motor,  motorC,               left_m, tmotorNxtEncoderClosedLoop,           !!*//
//*!!                                                                            !!*//
//*!!Start automatically generated configuration code.                           !!*//
const tSensors us1                  = (tSensors) S1;   //sensorSONAR        //*!!!!*//
const tSensors us2                  = (tSensors) S2;   //sensorSONAR        //*!!!!*//
const tSensors us3                  = (tSensors) S3;   //sensorSONAR        //*!!!!*//
const tMotor   right_m              = (tMotor) motorA; //tmotorNxtEncoderClosedLoop //*!!!!*//
const tMotor   left_m               = (tMotor) motorC; //tmotorNxtEncoderClosedLoop //*!!!!*//
//*!!CLICK to edit 'wizard' created sensor & motor configuration.                !!*//

/************************************************************************************/
// nxt_particles.c - an application that performs waypoint navigation while remaining
// localised based on sonar readings.
//
// Gordon Wyeth
// 24 November 2007
//
// See http://www.itee.uq.edu.au/~wyeth/nxt for details.
/************************************************************************************/

/********************************** Defines *****************************************/

// Particle filter parameters and settings
#define ENC_NOISE 0.15        // noise as fraction of commanded distance or angle
#define SENS_DIST_NOISE 35.0  // noise of sensor reading in mm
#define PART_XY_NOISE 50.0		// noise in positional placement of robot in mm
#define PART_Q_NOISE 0.0523  	// noise in positional placement of robot in rad
#define NUM_PRTCL 100					// number of particles *** NOTE update floating point value below as well
#define F_NUM_PRTCL 100.0     // floating point representation of number of particles *** change with above
#define ESS_THRESH 0.2				// resampling threshold

// Physical characteristics of robot
#define NUM_SENSORS 3					// number of sensors
#define RADIUS 26.0						// tyre radius
#define ROBOT_DIAM 110.0			// distance between turning point of wheels
#define MM_TO_ENC (180.0 / (PI * RADIUS))
#define DEG_TO_ENC (ROBOT_DIAM / (2.0 * RADIUS))

// Starting position and angle
#define START_X 300.0
#define START_Y 300.0
#define START_Q 0.0

// Acceleration and top speed of movement
#define MOTOR_POWER 60				// maximum power to motor (max value 100)
#define MAX_SPEED 600					// maximum speed of movement (max value 1000)

// Handy constants
#define TWO_PI 6.28318530718
#define PI_ON_TWO 1.5707963268
#define BIG_NUMBER 999999.0

/****************************** Typedefs ********************************/

// Choices for display while robot is operating
typedef enum {TEXT, ARROW, CLOUD} disp_types;

/****************************** Constants *******************************/

// Map information
const int		num_xwall = 1;		// Number of walls parallel to x-axis
const float xwall[num_xwall] = {0.0};	// y-values of walls parallel to x-axis
const int		num_ywall = 1;		// Number of walls parallel to y-axis
const float ywall[num_ywall] = {1250.0}; // x-values of walls parallel to y-axis
const int		num_tgts = 2;			// Number of cylindrical targets (posts, coffee cups)
const float xtgt[num_tgts] = {550.0, -100.0}; // x-coords of targets
const float ytgt[num_tgts] = {1000.0, 650.0}; // y-coords of targets
const float tgt_rad[num_tgts] = {45.0, 45.0}; // radii of targets

// Mission data
const int num_waypts = 4;			// Number of waypoints
const float waypt_x[num_waypts] = {800.0, 800.0, 300.0, 300.0}; 	// x coordinates of way points
const float waypt_y[num_waypts] = {350.0, 650.0, 650.0, 350.0};	// y coordinates of way points
const int num_segments = 3;
const bool cycle_waypoints = true;

// Sensors
const float sens_x[3] = {-60.0, 60.0, -60.0}; // x coords of sensors in robot coord frame
const float sens_y[3] = {-85.0, 0.0, 85.0}; // y coords of sensors in robot coord frame
const float sens_q[3] = {-PI_ON_TWO, 0.0, PI_ON_TWO};	// angle of sensors relative to robot coord frame

// Display settings
const float disp_scale = 20.0;
const disp_types disp_type = CLOUD;

// Precompute some handy numbers
const float	ess_thresh = ESS_THRESH * F_NUM_PRTCL;
const float	sens_dist_noise_2sq = 2.0 * SENS_DIST_NOISE * SENS_DIST_NOISE;

/****************************** Globals **********************************/

// Particle filter storage
float particle_x [NUM_PRTCL];
float particle_y [NUM_PRTCL];
float particle_q [NUM_PRTCL];
float particle_w [NUM_PRTCL];

// Sensor readings
float range[NUM_SENSORS];

// Interprocess variables
float avg_x, avg_y, avg_q;  // Pose reported by filter
float dist, dq;							// Commanded distance and angle from motion planner
int wait_for_filter, wait_for_drive;  // Semaphores for synchronisation
string cmd;									// String with last command as text for debugging

/******************************************************************************
*															Function definitions                            *
******************************************************************************/

/*************************** Utility functions *******************************/

// float uniform_rand() - Produces floating point random number between [0,1)
float uniform_rand()
{
	return ((float)(random(32766))/ 32767.0);
}

// float normal_rand() - Produces random number for Gaussian distribution with mean = 0 and sd = 1.
// Based on Box-Muller transformation.
float normal_rand()
{
	return sqrt(-2.0 * log(uniform_rand())) * cos(2.0 * PI * uniform_rand());
}

// void atan2(float y, float x, float &q) - Computes atan2() but returns value in parameter
INLINE void atan2(float y, float x, float &q)
{
	if (x > 0.0) {
		q = atan(y/x);
		return;
	}
	if (x < 0.0) {
		if (y >= 0.0) {
			q = PI + atan(y/x);
			return;
		}
		q = -PI + atan(y/x);
		return;
	}
	if (y > 0.0) { // x == 0
		q = PI_ON_TWO;
		return;
	}
	if (y < 0.0) {
		q = -PI_ON_TWO;
		return;
	}
	q = 0.0; // Should really be undefined
	return;
}

// void limit_ang(float &angle) - brings an angle back between +- PI
// BEWARE: make sure this is called frequently on all angular variables or else it can take a
// long time to unwind a angular number that has been rotated many times. Could be implemented better.
INLINE void limit_ang(float &angle)
{
	while (angle > PI)
		angle -= TWO_PI;
	while (angle < -PI)
		angle += TWO_PI;
}

/************************* Motion planning functions *****************************/

// void do_move(tMotor m, int d) - Command the motor m to move through the
// prescribed encoder counts d and block until the movement is complete.
void do_move(tMotor m, int d)
{
	nMotorEncoderTarget[m] = d;
	motor[m] = MOTOR_POWER;
	while (nMotorRunState[m] != 0) {
		wait1Msec(1);
	}
	motor[m] = 0;
}

// void go_fwd (float dist_mm) - Move robot forward the given distance in mm. Blocks
// til movement done.
void go_fwd (float dist_mm)
{
	int dist;

	dist = (int)(dist_mm * MM_TO_ENC);
	nSyncedMotors = synchAC;
	nSyncedTurnRatio = +100;
	do_move (right_m, dist);
}

// void turn_left (float ang_deg) - Turns robot left given angle in degrees. Blocks
// til movement done.
void turn_left (float ang_deg)
{
	int dist;

	dist = (int)(ang_deg * DEG_TO_ENC);
	if (dist > 0) {
		nSyncedMotors = synchAC;
		nSyncedTurnRatio = -100;
		do_move (right_m, dist);
	}
}

// void turn_right (float ang_deg) - Turns robot right given angle in degrees. Blocks
// til movement done.
void turn_right (float ang_deg)
{
	int dist;

	dist = (int)(ang_deg * DEG_TO_ENC);
	if (dist > 0) {
		nSyncedMotors = synchCA;
		nSyncedTurnRatio = -100;
		do_move (left_m, dist);
	}
}

// task drive() - Main routine in motion planner. Moves robot from waypoint to waypoint,
// returning to first waypoint if cycle_waypoints is true. Motion between waypoints is
// divided into segments to allow localisation as the target is approached. Number of
// segments is defined in num_segments. drive() waits for filter to produce localisation
// estimate before attempting each segment. drive() also blocks filter until a motion is
// complete. Blocking is accomplished by semaphores, but be careful of deadlocks!!
task drive()
{
	float dx, dy, dq_degrees;
	int i, j;

	bFloatDuringInactiveMotorPWM = false;
	nMotorPIDSpeedCtrl[left_m] = mtrSpeedReg;
	nMotorPIDSpeedCtrl[right_m] = mtrSpeedReg;
	nPidUpdateInterval = 5;
	nMaxRegulatedSpeed = MAX_SPEED;

	do {
		for (i = 0; i < num_waypts; i++) {
			for (j = 0; j < num_segments; j++) {

				// Wait for most up to date values
				wait_for_filter = 1;
				while (wait_for_filter > 0) {
					wait1Msec(1);
				}

				dx = waypt_x[i] - avg_x;
				dy = waypt_y[i] - avg_y;
				dist = sqrt(dx * dx + dy * dy) / (float)(num_segments - j);
				atan2(dy, dx, dq);
				dq -= avg_q;
				limit_ang(dq);
				dq_degrees = dq / PI * 180.0;
				StringFormat(cmd, "Q:%3.0f D:%3.0f", dq_degrees, dist);
				if (dq > 0.0) {
					turn_left (dq_degrees);
				} else if (dq < 0.0) {
					turn_right (-dq_degrees);
				}
				go_fwd (dist);

				// Finished moving so OK to run filter again
				if (wait_for_drive > 0) {
					wait_for_drive--;
				}
			}
		}
	} while (cycle_waypoints);
}

/*************************** Display functions **************************/

// int xscale(float x) - Convert from map to display scale
int xscale(float x)
{
	return ((int)(x / disp_scale));
}

// int yscale(float y) - Convert from map to display scale
int yscale(float y)
{
	return ((int)(y / disp_scale));
}

// void update_display() - Update the debug display. Constant parameter disp_type sets
// which display to update. TEXT gives text printout of key values. ARROW gives a quiver
// plot of particles centred on the screen (principally for debugging heading issues).
// CLOUD is the most useful with plots of particle positions, the map, sensor readings
// and a text printout of the current movement.
// Constant value disp_scale sets the scale from world coords to pixels.
void update_display()
{
	int i, n;
	int x1,y1,x2,y2;
	float sx, sy;
	float cpq, spq;

	eraseDisplay();
	if (disp_type == TEXT) {
		// Display printout of key variables
		nxtDisplayTextLine(1,"SR:%f",range[0]);
		nxtDisplayTextLine(2,"SF:%f",range[1]);
		nxtDisplayTextLine(3,"SL:%f",range[2]);
		nxtDisplayTextLine(4,"AX:%f",avg_x);
		nxtDisplayTextLine(5,"AY:%f",avg_y);
		nxtDisplayTextLine(6,"AQ:%f",avg_q / PI * 180.0);
		nxtDisplayTextLine(7,"CMD:%s",cmd);
	} else if (disp_type == ARROW) {
		// Display quiver plot of particles with headings centred on screen
		for (i = 0; i < NUM_PRTCL; i++) {
			x1 = 50 + xscale(particle_x[i] - avg_x);
			y1 = 32 + yscale(particle_y[i] - avg_y);
			x2 = 50 + xscale(particle_x[i] - avg_x + 400.0 * cos(particle_q[i]));
			y2 = 32 + yscale(particle_y[i] - avg_y + 400.0 * sin(particle_q[i]));
	    nxtDrawLine(x1,y1,x2,y2);
		}
	} else if (disp_type == CLOUD){
		// Display sensor readings
		for (n = 0; n < NUM_SENSORS; n++) {
			if (range[n] != BIG_NUMBER) {
				cpq = cos(avg_q);
				spq = sin(avg_q);
				sx = avg_x + sens_x[n] * cpq - sens_y[n] * spq ;
				sy = avg_y + sens_x[n] * spq + sens_y[n] * cpq ;
				x1 = xscale(sx);
				y1 = yscale(sy);
				x2 = xscale(sx + range[n]*cos(avg_q + sens_q[n]));
				y2 = yscale(sy + range[n]*sin(avg_q + sens_q[n]));
		    nxtDrawLine(x1,y1,x2,y2);
			}
		}
		// Display particle cloud
		for (i = 0; i < NUM_PRTCL; i++) {
			x1 = xscale(particle_x[i]);
			y1 = yscale(particle_y[i]);
	    nxtSetPixel(x1,y1);
		}
		// Display map targets
		for (i = 0; i < num_tgts; i++) {
			x1 = xscale(xtgt[i] - 2);
			y1 = yscale(ytgt[i] + 2);
	    nxtDrawCircle(x1,y1,2*(xscale(tgt_rad)));
	  }
	  // Draw walls
		for (i = 0; i < num_xwall; i++) {
			x1 = 0;
			y1 = yscale(xwall[i]);
			x2 = 99;
	    nxtDrawLine(x1,y1,x2,y1);
	  }
		for (i = 0; i < num_ywall; i++) {
			x1 = xscale(ywall[i]);
			y1 = 0;
			y2 = 63;
	    nxtDrawLine(x1,y1,x1,y2);
	  }
	  // Display current command
	  nxtDisplayStringAt(2,63,cmd);
	}
}

/***************************** Particle filter functions *********************/

// void read_sensors () - Read the sensors and put the values in range. Invalid
// sensor readings are set to BIG_NUMBER
void read_sensors ()
{
	int n;
	int raw_s[3];

  raw_s[0] = SensorRaw[us3];
  raw_s[1] = SensorRaw[us1];
  raw_s[2] = SensorRaw[us2];

	for (n = 0; n < NUM_SENSORS; n++) {
		if ((raw_s[n] > 100) || (raw_s[n] < 27)) {
			range[n] = BIG_NUMBER;
			raw_s[n] = 255;
		} else {
			range[n] = (float)raw_s[n] * 10.0;
		}
	}
}

// void resample () - Resample the particles using Select with Replacement.
void resample ()
{
	int i, j;
	float uniform_weight;
	float new_x [NUM_PRTCL];
	float new_y [NUM_PRTCL];
	float new_q [NUM_PRTCL];
	int index[NUM_PRTCL];
	int x[NUM_PRTCL];
	float q [NUM_PRTCL];
	float r [NUM_PRTCL];

	// Produce cumulative distribution
	q[0] = particle_w[0];
	for (i = 1; i < NUM_PRTCL; i++) {
		q[i] = q[i-1] + particle_w[i];
	}

	// Produce a list of random integers with an ordering index
	for (i = 0; i < NUM_PRTCL; i++) {
		x[i] = random(32766);
		index[i] = 0;
		for (j = 0; j < i; j++) {
			if (x[j] < x[i]) {
				if (index[i] <= index[j]) {
					index[i] = index[j] + 1;
				}
			} else {
				index[j]++;
			}
		}
	}
	// Put numbers out in order using index, scale and cast list to floats
	for (i = 0; i < NUM_PRTCL; i++) {
		r[index[i]] = (float)(x[i]) / 32767.0;
	}

	// Make copies of particles as many times as the sorted random numbers appear
	// between the cumulative distribution values
	i = 0;
	j = 0;
  while (i < NUM_PRTCL) {
  	if (r[i] < q[j]) {
  		new_x[i] = particle_x[j];
  		new_y[i] = particle_y[j];
  		new_q[i] = particle_q[j];
      i = i + 1;
    } else {
      j = j + 1;
    }
	}

	// Put the copies into the particle list and reset weights
	uniform_weight = 1.0 / F_NUM_PRTCL;
	for (i = 0; i < NUM_PRTCL; i++) {
		particle_x[i] = new_x[i];
		particle_y[i] = new_y[i];
		particle_q[i] = new_q[i];
		particle_w[i] = uniform_weight;
	}
}

// float get_reading(int n, float px, float py, float pq) - Work out what sensor
// reading for sensor number n should be if robot is in pose (px, py, pq) using
// information in map
float get_reading(int n, float px, float py, float pq)
{
	const float sens_width = PI / 3.0;

	float least_dist = BIG_NUMBER;
	int i;
	float dx, dy, dist, ang;
	float spq, cpq;

	// Update particle position to account sensor position
	// wrt to turning axis of robot.
	cpq = cos(pq);
	spq = sin(pq);
	px += sens_x[n] * cpq - sens_y[n] * spq ;
	py += sens_x[n] * spq + sens_y[n] * cpq ;
	pq += sens_q[n];

	// Get a reading to any targets
	for (i = 0; i < num_tgts; i++) {
		dx = xtgt[i] - px;
		dy = ytgt[i] - py;
		dist = sqrt(dx * dx + dy * dy) - tgt_rad[i];
		atan2 (dy, dx, ang);
		ang = pq - ang;
		limit_ang (ang);
		if ((dist < least_dist) && (abs(ang) < sens_width)) {
			least_dist = dist;
		}
	}

	// Get a reading to any walls running in x direction (constant y value)
	for (i = 0; i < num_xwall; i++) {
		spq = sin(pq);
		if (spq != 0.0) {
			dist = (xwall[i] - py) / spq;
			if ((dist > 0) && (dist < least_dist)) {
				least_dist = dist;
			}
		}
	}

	// Get a reading to any walls running in y direction (constant x value)
	for (i = 0; i < num_ywall; i++) {
		cpq = cos(pq);
		if (cpq != 0.0) {
			dist = (ywall[i] - px) / cpq;
			if ((dist > 0) && (dist < least_dist)) {
				least_dist = dist;
			}
		}
	}
	return (least_dist);
}

// void init_particles(int particle_seed) - Initialise particles with a spread
// to express uncertainty in robot placement at start of test
void init_particles(int particle_seed)
{
	int i;
	float uniform_weight;

	srand(particle_seed);
	uniform_weight = 1.0 / F_NUM_PRTCL;
	for (i = 0; i < NUM_PRTCL; i++) {
		particle_x[i] = START_X + (normal_rand() * PART_XY_NOISE);
		particle_y[i] = START_Y + (normal_rand() * PART_XY_NOISE);
		particle_q[i] = START_Q + (normal_rand() * PART_Q_NOISE);
		particle_w[i] = uniform_weight;
	}
}

// void predict_particles() - Predict the position of particles based on commanded movement
// and adding process noise
void predict_particles()
{
	int i;
	float approx_ang;
	float noisy_dist, noisy_ang;

	// Predict
	for (i = 0; i < NUM_PRTCL; i++) {
		noisy_dist = dist * (1.0 + normal_rand() * ENC_NOISE);
		noisy_ang = dq * (1.0 + normal_rand() * ENC_NOISE);
		approx_ang = particle_q[i] + noisy_ang;
		particle_x[i] += noisy_dist * cos(approx_ang);
		particle_y[i] += noisy_dist * sin(approx_ang);
		particle_q[i] += noisy_ang;
		limit_ang(particle_q[i]);
	}
}

// void update_weights() - Update the weights based on the current range readings
// and the quality of match of the readings to the map
void update_weights()
{
	int i, n;
	float map_range;
	float dist_err;

	for (n = 0; n < NUM_SENSORS; n++) {
	  if (range[n] != BIG_NUMBER) {
			for (i = 0; i < NUM_PRTCL; i++) {
				map_range = get_reading(n, particle_x[i], particle_y[i], particle_q[i]);
				if (map_range != BIG_NUMBER) {
					dist_err = range[n] - map_range;
			    particle_w[i] *= exp(-dist_err * dist_err / sens_dist_noise_2sq);
			  }
		  }
		}
	}
}

// float normalise_weights() - Normalise the weights to sum to 1.0. Returns original sum
// value principally to check for all zero weights which means we're lost
float normalise_weights()
{
	float sum;
	int i;

	sum = 0.0;
	for (i = 0; i < NUM_PRTCL; i++) {
    sum += particle_w[i];
	}
	if (sum != 0.0) {
		for (i = 0; i < NUM_PRTCL; i++) {
	    particle_w[i] /= sum;
	  }
	}
	return sum;
}

// void update_averages() - Computes the weighted average of the particles. NOTE Prone
// to error if multi-modal distribution. Should really find mode and only use particles
// near mode.
void update_averages()
{
	int i;
	float a_x, a_y, a_cq, a_sq;

	a_x  = 0.0;
	a_y  = 0.0;
	a_cq = 0.0;
	a_sq = 0.0;
	for (i = 0; i < NUM_PRTCL; i++) {
    a_x += particle_x[i] * particle_w[i];
    a_y += particle_y[i] * particle_w[i];
    // Compute using quadrature components to prevent problems at +/- PI
    a_cq += cos(particle_q[i]) * particle_w[i];
    a_sq += sin(particle_q[i]) * particle_w[i];
	}
	avg_x = a_x;
	avg_y = a_y;
	atan2(a_sq, a_cq, avg_q);
}

// float compute_ess() - Returns ess calculation to check information quality
// of particles
float compute_ess()
{
	int i;
	float sum, temp;

	// Check ESS
	sum = 0.0;
	for (i = 0; i < NUM_PRTCL; i++) {
		temp = F_NUM_PRTCL * particle_w[i] - 1.0;
		sum += temp * temp;
	}
	return(F_NUM_PRTCL / (1.0 + sum / F_NUM_PRTCL));
}

/************************** Main task definition *****************************/

task main ()
{
	int i;
	float ess;
	float sum;

	wait10Msec(500);

	// Initialise position
	avg_x = START_X;
	avg_y = START_Y;
	avg_q = START_Q;

	// Initialise particles with a random seed
	init_particles(612);

	// Set the initial movement to zero
	dist = 0.0;
	dq = 0.0;

	// Start the waypoint finding thread - blocks until filter first pass
	StartTask (drive);

	while (true)
	{

		// Predict the position of the particles based on the last movement command
		predict_particles();

		// Sense
		read_sensors();

		// Update weights
		update_weights();

		// Normalise and check for zero weights
		sum = normalise_weights();

		// If all weights are zero then we are REALLY lost
		if (sum == 0.0) {
			StopTask(drive);
			motor[left_m] = 0;
			motor[right_m] = 0;
			update_display();
			nxtDisplayTextLine(0,"Hopelessly lost!!");
			while(true) {
			}
		}

		// Compute weighted average position of particles
		update_averages();

		// Let the blocked movement task run now we are localised
		if (wait_for_filter > 0)
			wait_for_filter--;

		// Resample if necessary
		ess = compute_ess();
		if (ess < ess_thresh) {
			resample();
		}

		// Update the display
		update_display();

		// Wait for movement to finish before looping
		wait_for_drive = 1;
		while (wait_for_drive > 0) {
			wait1Msec(1);
		}
	}
}
