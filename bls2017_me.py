# Copyright 2018 Google LLC. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Basic nonlinear transform coder for RGB images.

This is a close approximation of the image compression model published in:
J. Ballé, V. Laparra, E.P. Simoncelli (2017):
"End-to-end Optimized Image Compression"
Int. Conf. on Learning Representations (ICLR), 2017
https://arxiv.org/abs/1611.01704

With patches from Victor Xing <victor.t.xing@gmail.com>

This is meant as 'educational' code - you can use this to get started with your
own experiments. To reproduce the exact results from the paper, tuning of hyper-
parameters may be necessary. To compress images with published models, see
`tfci.py`.

This script requires TFC v2 (`pip install tensorflow-compression==2.*`).
"""

import argparse
import glob
import sys
from absl import app
from absl.flags import argparse_flags
import tensorflow as tf
import tensorflow_compression as tfc
import numpy as np

def compress(args):

  model = tf.keras.models.load_model(args.model_path)

#################################################
  specific_layer = model.get_layer('analysis')
  fp_w = open("bin/ana_w.bin", "wb+")
  fp_b = open("bin/ana_b.bin", "wb+")

  weights_ttt = specific_layer.layers[1].variables
  weight_values = [weight.numpy() for weight in weights_ttt]
  real_part = weight_values[4]
  imag_part = weight_values[3]
  complex_tensor = tf.complex(real_part, imag_part)
  z1 = tf.constant([9.0 + 0.0j], dtype=tf.complex64)
  result0 = tf.math.multiply(complex_tensor, z1)
  irfft2d = tf.signal.irfft2d(result0, fft_length=[9,9])
  w_conv0 = tf.transpose(irfft2d, perm=[2,3,0,1])
  b_conv0 = weight_values[0]
  fp_w.write(w_conv0)
  fp_b.write(b_conv0)

  w_gdn0 = weight_values[2]
  b_gdn0 = weight_values[1]
  fp_w.write(w_gdn0*w_gdn0)
  fp_b.write(b_gdn0*b_gdn0)

  weights_ttt = specific_layer.layers[2].variables
  weight_values = [weight.numpy() for weight in weights_ttt]
  real_part = weight_values[4]
  imag_part = weight_values[3]
  complex_tensor = tf.complex(real_part, imag_part)
  z1 = tf.constant([5.0 + 0.0j], dtype=tf.complex64)
  result0 = tf.math.multiply(complex_tensor, z1)
  irfft2d = tf.signal.irfft2d(result0, fft_length=[5,5])
  w_conv1 = tf.transpose(irfft2d, perm=[2,3,0,1])
  b_conv1 = weight_values[0]
  fp_w.write(w_conv1)
  fp_b.write(b_conv1)

  w_gdn1 = weight_values[2]
  b_gdn1 = weight_values[1]
  fp_w.write(w_gdn1*w_gdn1)
  fp_b.write(b_gdn1*b_gdn1)

  weights_ttt = specific_layer.layers[3].variables
  weight_values = [weight.numpy() for weight in weights_ttt]
  real_part = weight_values[1]
  imag_part = weight_values[0]
  complex_tensor = tf.complex(real_part, imag_part)
  z1 = tf.constant([5.0 + 0.0j], dtype=tf.complex64)
  result0 = tf.math.multiply(complex_tensor, z1)
  irfft2d = tf.signal.irfft2d(result0, fft_length=[5,5])
  w_conv2 = tf.transpose(irfft2d, perm=[2,3,0,1])
  fp_w.write(w_conv2)

  fp_w.close()
  fp_b.close()

########################################################################
  specific_layer = model.get_layer('synthesis')
  fp_w = open("bin/syn_w.bin", "wb+")
  fp_b = open("bin/syn_b.bin", "wb+")

  weights_ttt = specific_layer.layers[0].variables
  weight_values = [weight.numpy() for weight in weights_ttt]
  real_part = weight_values[4]
  imag_part = weight_values[3]
  complex_tensor = tf.complex(real_part, imag_part)
  z1 = tf.constant([5.0 + 0.0j], dtype=tf.complex64)
  result0 = tf.math.multiply(complex_tensor, z1)
  irfft2d = tf.signal.irfft2d(result0, fft_length=[5,5])
  w_conv0 = tf.transpose(irfft2d, perm=[2,3,0,1])
  w_conv0 = tf.transpose(w_conv0, perm=[0,1,3,2])
  b_conv0 = weight_values[0]
  rotated_weights = np.rot90(w_conv0, k=2)
  fp_w.write(tf.transpose(rotated_weights, perm=[0,1,3,2]))
  fp_b.write(b_conv0)

  w_igdn0 = weight_values[2]
  b_igdn0 = weight_values[1]
  fp_w.write(w_igdn0*w_igdn0)
  fp_b.write(b_igdn0*b_igdn0)

  weights_ttt = specific_layer.layers[1].variables
  weight_values = [weight.numpy() for weight in weights_ttt]
  real_part = weight_values[4]
  imag_part = weight_values[3]
  complex_tensor = tf.complex(real_part, imag_part)
  z1 = tf.constant([5.0 + 0.0j], dtype=tf.complex64)
  result0 = tf.math.multiply(complex_tensor, z1)
  irfft2d = tf.signal.irfft2d(result0, fft_length=[5,5])
  w_conv1 = tf.transpose(irfft2d, perm=[2,3,0,1])
  w_conv1 = tf.transpose(w_conv1, perm=[0,1,3,2])
  b_conv1 = weight_values[0]
  rotated_weights = np.rot90(w_conv1, k=2)
  fp_w.write(tf.transpose(rotated_weights, perm=[0,1,3,2]))
  fp_b.write(b_conv1)

  w_igdn1 = weight_values[2]
  b_igdn1 = weight_values[1]

  fp_w.write(w_igdn1*w_igdn1)
  fp_b.write(b_igdn1*b_igdn1)

  weights_ttt = specific_layer.layers[2].variables
  weight_values = [weight.numpy() for weight in weights_ttt]
  real_part = weight_values[2]
  imag_part = weight_values[1]
  complex_tensor = tf.complex(real_part, imag_part)
  z1 = tf.constant([9.0 + 0.0j], dtype=tf.complex64)
  result0 = tf.math.multiply(complex_tensor, z1)
  irfft2d = tf.signal.irfft2d(result0, fft_length=[9,9])
  w_conv1 = tf.transpose(irfft2d, perm=[2,3,0,1])
  w_conv1 = tf.transpose(w_conv1, perm=[0,1,3,2])
  b_conv1 = weight_values[0]
  rotated_weights = np.rot90(w_conv1, k=2)
  fp_w.write(tf.transpose(rotated_weights, perm=[0,1,3,2]))
  fp_b.write(b_conv1)

  fp_w.close()
  fp_b.close()
################################################################

def parse_args(argv):
  """Parses command line arguments."""
  parser = argparse_flags.ArgumentParser(
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)

  parser.add_argument(
      "--model_path", default="bls2017",
      help="Path where to save/load the trained model.")

  args = parser.parse_args(argv[1:])

  return args


def main(args):
    compress(args)

if __name__ == "__main__":
  app.run(main, flags_parser=parse_args)
