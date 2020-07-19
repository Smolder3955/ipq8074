#
# @@-COPYRIGHT-START-@@
#
# Copyright (c) 2015-2016 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
# @@-COPYRIGHT-END-@@
#

from setuptools import setup, find_packages
import os

here = os.path.abspath(os.path.dirname(__file__))
README = open(os.path.join(here, 'README.rst')).read()

# Cool trick to automatically pull in install_requires values directly from
# your requirements.txt file but you need to have pip module available
try:
    from pip.req import parse_requirements
    try:
        # Newer versions of pip need a session object when parsing requirements.
        from pip.download import PipSession
        install_reqs = parse_requirements(os.path.join(here, 'requirements.txt'),
                                          session=PipSession())
    except ImportError:
        # Must be an older version of pip
        install_reqs = parse_requirements(os.path.join(here, 'requirements.txt'))

    reqs = [str(ir.req) for ir in install_reqs]
except Exception as e:
    print "Failed to parse requirements: " + str(e)
    # I suggest you install pip on your system so you don't have to
    # manually update this list along with your requirements.txt file
    reqs = []


setup(name='whcviz',
      version='3.0.0',
      description="Whole Home Coverage - Visualization GUI",
      long_description=README,
      classifiers=[
          "Development Status :: 4 - Beta",
      ],
      keywords='whc steering',
      author="Qualcomm Atheros, Inc.",
      url="https://www.qualcomm.com/",
      packages=find_packages(exclude=['tests']),
      scripts = ['scripts/%s' % f for f in os.listdir('scripts')],
      include_package_data=True,
      zip_safe=True,
      install_requires=reqs,
      setup_requires = ["setuptools_git >= 0.3", "flake8", "pep8 == 1.5.7"],
      tests_require=['nose', 'coverage', 'mock', 'whcdiag', 'whcmvc'],
      entry_points="""
      # -*- Entry points: -*-
      """,
      )
