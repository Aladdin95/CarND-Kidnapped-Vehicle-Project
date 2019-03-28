/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <limits.h>

#include "helper_functions.h"

using std::string;
using std::vector;
using std::normal_distribution;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  
  std::default_random_engine gen;
  normal_distribution<double> dist_x(x, std[0]);
  normal_distribution<double> dist_y(y, std[1]);
  normal_distribution<double> dist_theta(theta, std[2]);
  
  num_particles = 100;  // TODO: Set the number of particles
  particles.resize(num_particles);
  
  for (int i = 0; i < num_particles; ++i) 
  {
    particles[i].id = i;
    particles[i].x = dist_x(gen);
    particles[i].y = dist_y(gen);
    particles[i].theta = dist_theta(gen);
    particles[i].weight = 1;
  }
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
  std::default_random_engine gen;
  normal_distribution<double> dist_x(0, std_pos[0]);
  normal_distribution<double> dist_y(0, std_pos[1]);
  normal_distribution<double> dist_theta(0, std_pos[2]);

  for (int i = 0; i < num_particles; ++i) 
  {
    if(fabs(yaw_rate) < 0.00001)
    {
      particles[i].x += velocity * delta_t * cos(particles[i].theta) + dist_x(gen);
      particles[i].y += velocity * delta_t * sin(particles[i].theta) + dist_y(gen);
    }
    else
    {
      particles[i].x += velocity/yaw_rate * (sin(particles[i].theta + yaw_rate*delta_t) - sin(particles[i].theta)) + dist_x(gen);
      particles[i].y += velocity/yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + yaw_rate*delta_t)) + dist_y(gen);
      particles[i].theta += yaw_rate*delta_t + dist_theta(gen);
    }
  }
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
  if(!predicted.size()) return;
  for(unsigned int i=0; i<observations.size(); ++i)
  {
    double d = dist(observations[i].x, observations[i].y, predicted[0].x, predicted[0].y);
    double dt;
    int id = predicted[0].id;
    for(unsigned int j=1; j<predicted.size(); ++j)
    {
      dt = dist(observations[i].x, observations[i].y, predicted[j].x, predicted[j].y);
      if(dt < d)
      {
        d = dt;
        id = predicted[j].id;
      }
    }
    observations[i].id = id;
  }
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
  
  for (int i = 0; i < num_particles; ++i)
  {
    vector<LandmarkObs> predicted;
    predicted.reserve( map_landmarks.landmark_list.size() );
    for (unsigned int j = 0; j < map_landmarks.landmark_list.size(); ++j)
    {
      //if(fabs(map_landmarks.landmark_list[j].x_f - particles[i].x) <= sensor_range &&
      //   fabs(map_landmarks.landmark_list[j].y_f - particles[i].y) <= sensor_range)
          predicted.push_back(LandmarkObs{ map_landmarks.landmark_list[j].id_i,
            map_landmarks.landmark_list[j].x_f,
            map_landmarks.landmark_list[j].y_f });
    }
    
    vector<LandmarkObs> obs(observations.size());
    for (unsigned int j = 0; j < observations.size(); ++j)
    {
      double x = cos(particles[i].theta)*observations[j].x - sin(particles[i].theta)*observations[j].y + particles[i].x;
      double y = sin(particles[i].theta)*observations[j].x + cos(particles[i].theta)*observations[j].y + particles[i].y;
      obs[j] = LandmarkObs { observations[j].id, x, y };
    }
    
    dataAssociation(predicted, obs);
    
    particles[i].weight = 1;
    for (unsigned int j = 0; j < obs.size(); ++j) 
    {
      unsigned int k = 0;
      for(; k<predicted.size(); ++k)
        if(obs[j].id == predicted[k].id) break;
      
      if(k == particles.size()) continue;
      double w = 1/(2*M_PI*std_landmark[0]*std_landmark[1]) * 
        exp(-1*(pow(predicted[k].x-obs[j].x,2)/(2*pow(std_landmark[0],2))+pow(predicted[k].y-obs[j].y,2)/(2*pow(std_landmark[1],2))));
      particles[i].weight *= w;
    }
  }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
  double sum_weight = 0;
  for (int i = 0; i < num_particles; ++i)
    sum_weight += particles[i].weight;
  
  vector<int> intweights(num_particles);
  for (int i = 0; i < num_particles; ++i)
    intweights[i] = int((particles[i].weight / sum_weight) * INT_MAX);

  std::default_random_engine gen;
  std::discrete_distribution<> d(intweights.begin(), intweights.end());
  
  std::vector<Particle> new_particles(particles.begin(), particles.end());
  for (int i = 0; i < num_particles; ++i)
    particles[i] = new_particles[d(gen)];
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}