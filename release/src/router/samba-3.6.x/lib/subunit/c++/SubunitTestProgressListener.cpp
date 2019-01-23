/*  Subunit test listener for cppunit (http://cppunit.sourceforge.net).
 *  Copyright (C) 2006  Robert Collins <robertc@robertcollins.net>
 *
 *  Licensed under either the Apache License, Version 2.0 or the BSD 3-clause
 *  license at the users choice. A copy of both licenses are available in the
 *  project source as Apache-2.0 and BSD. You may not use this file except in
 *  compliance with one of these two licences.
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under these licenses is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the license you chose for the specific language governing permissions
 *  and limitations under that license.
 */

#include <cppunit/Exception.h>
#include <cppunit/Test.h>
#include <cppunit/TestFailure.h>
#include <cppunit/TextOutputter.h>
#include <iostream>

// Have to be able to import the public interface without config.h.
#include "SubunitTestProgressListener.h"
#include "config.h"
#include "subunit/child.h"


CPPUNIT_NS_BEGIN


void 
SubunitTestProgressListener::startTest( Test *test )
{
  subunit_test_start(test->getName().c_str());
  last_test_failed = false;
}

void 
SubunitTestProgressListener::addFailure( const TestFailure &failure )
{
  std::ostringstream capture_stream;
  TextOutputter outputter(NULL, capture_stream);
  outputter.printFailureLocation(failure.sourceLine());
  outputter.printFailureDetail(failure.thrownException());

  if (failure.isError())
      subunit_test_error(failure.failedTestName().c_str(),
        		 capture_stream.str().c_str());
  else
      subunit_test_fail(failure.failedTestName().c_str(),
                        capture_stream.str().c_str());
  last_test_failed = true;
}

void 
SubunitTestProgressListener::endTest( Test *test)
{
  if (!last_test_failed)
      subunit_test_pass(test->getName().c_str());
}


CPPUNIT_NS_END
