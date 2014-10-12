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
#ifndef CPPUNIT_SUBUNITTESTPROGRESSLISTENER_H
#define CPPUNIT_SUBUNITTESTPROGRESSLISTENER_H

#include <cppunit/TestListener.h>


CPPUNIT_NS_BEGIN


/*! 
 * \brief TestListener that outputs subunit
 * (http://www.robertcollins.net/unittest/subunit) compatible output.
 * \ingroup TrackingTestExecution
 */
class CPPUNIT_API SubunitTestProgressListener : public TestListener
{
public:
 
  SubunitTestProgressListener() {}
  
  void startTest( Test *test );

  void addFailure( const TestFailure &failure );

  void endTest( Test *test );

private:
  /// Prevents the use of the copy constructor.
  SubunitTestProgressListener( const SubunitTestProgressListener &copy );

  /// Prevents the use of the copy operator.
  void operator =( const SubunitTestProgressListener &copy );

private:
  int last_test_failed;
};


CPPUNIT_NS_END

#endif  // CPPUNIT_SUBUNITTESTPROGRESSLISTENER_H

