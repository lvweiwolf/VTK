/*=========================================================================

Program:   Visualization Toolkit
Module:    TestRandomPOrderStatisticsMPI.cxx

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
/*
 * Copyright 2011 Sandia Corporation.
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the
 * U.S. Government. Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that this Notice and any
 * statement of authorship are reproduced on all copies.
 */
// .SECTION Thanks
// Thanks to Philippe Pebay for implementing this test.

#include <mpi.h>

#include "vtkOrderStatistics.h"
#include "vtkPOrderStatistics.h"

#include "vtkIdTypeArray.h"
#include "vtkIntArray.h"
#include "vtkMath.h"
#include "vtkMPIController.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkStdString.h"
#include "vtkStringArray.h"
#include "vtkTable.h"
#include "vtkTimerLog.h"
#include "vtkVariantArray.h"

#include "vtksys/CommandLineArguments.hxx"

struct RandomOrderStatisticsArgs
{
  int nVals;
  double stdev;
  bool quantize;
  int maxHistoSize;
  int* retVal;
  int ioRank;
};

// This will be called by all processes
void RandomOrderStatistics( vtkMultiProcessController* controller, void* arg )
{
  // Get test parameters
  RandomOrderStatisticsArgs* args = reinterpret_cast<RandomOrderStatisticsArgs*>( arg );
  *(args->retVal) = 0;

  // Get MPI communicator
  vtkMPICommunicator* com = vtkMPICommunicator::SafeDownCast( controller->GetCommunicator() );

  // Get local rank
  int myRank = com->GetLocalProcessId();

  // Seed random number generator
  vtkMath::RandomSeed( static_cast<int>( vtkTimerLog::GetUniversalTime() ) * ( myRank + 1 ) );

  // Generate an input table that contains samples of:
  // 1. A truncated Gaussian pseudo-random variable (vtkIntArray)
  // 2. A uniform pseudo-random variable of characters (vtkStringArray)
  vtkStdString columnNames[] = { "Rounded Normal Integer", "Uniform Character" };
  int nVariables = 2;

  // Prepare column of integers
  vtkIntArray* intArray = vtkIntArray::New();
  intArray->SetNumberOfComponents( 1 );
  intArray->SetName( columnNames[0] );
  
  // Prepare column of strings
  vtkStringArray* strArray = vtkStringArray::New();
  strArray->SetNumberOfComponents( 1 );
  strArray->SetName( columnNames[1] );
  
  // Store first values
  int v[2];
  v[0] = static_cast<int>( vtkMath::Round( vtkMath::Gaussian() * args->stdev ) );
  intArray->InsertNextValue( v[0] );
  
  v[1] = 96 + vtkMath::Ceil( vtkMath::Random() * 26 );
  char c = static_cast<char>( v[1] );
  vtkStdString s( &c, 1 );
  strArray->InsertNextValue( s );

  // Initialize local extrema
  int min_l[] = { v[0], v[1] };
  int max_l[] = { v[0], v[1] };
  
  // Continue up to nVals values have been generated
  for ( int r = 1; r < args->nVals; ++ r )
    {
    // Store new values
    v[0] = static_cast<int>( vtkMath::Round( vtkMath::Gaussian() * args->stdev ) );
    intArray->InsertNextValue( v[0] );
    
    v[1] = 96 + vtkMath::Ceil( vtkMath::Random() * 26 );
    c = static_cast<char>( v[1] );
    s = vtkStdString( &c, 1 );
    strArray->InsertNextValue( s );

    // Update local extrema
    for ( int i = 0; i < nVariables; ++ i )
      {
      if ( v[i] < min_l[i] )
        {
        min_l[i] = v[i];
        }
      else if ( v[i] > max_l[i] )
        {
        max_l[i] = v[i];
        }
      } // i
    } // r

  // Create input table
  vtkTable* inputData = vtkTable::New();
  inputData->AddColumn( intArray );
  inputData->AddColumn( strArray );

  // Clean up
  intArray->Delete();
  strArray->Delete();

  // Reduce extrema for all variables
  int min_g[2];
  int max_g[2];
  for ( int i = 0; i < nVariables; ++ i )
    {
    com->AllReduce( &min_l[i],
                    &min_g[i],
                    1,
                    vtkCommunicator::MIN_OP );

    com->AllReduce( &max_l[i],
                    &max_g[i],
                    1,
                    vtkCommunicator::MAX_OP );
    } // i

  if ( myRank == args->ioRank )
    {
    cout << "\n## Generated pseudo-random samples with following ranges:"
         << "\n   "
         << columnNames[0]
         << ": "
         << min_g[0]
         << " to "
         << max_g[0]
         << "\n   "
         << columnNames[1]
         << ": "
         << static_cast<char>( min_g[1] )
         << " to "
         << static_cast<char>( max_g[1] )
         << "\n";
    }

  // ************************** Order Statistics **************************

  // Synchronize and start clock
  com->Barrier();
  vtkTimerLog *timer=vtkTimerLog::New();
  timer->StartTimer();

  // Instantiate a parallel order statistics engine and set its ports
  vtkPOrderStatistics* pos = vtkPOrderStatistics::New();
  pos->SetInput( vtkStatisticsAlgorithm::INPUT_DATA, inputData );
  vtkMultiBlockDataSet* outputModelDS = vtkMultiBlockDataSet::SafeDownCast( pos->GetOutputDataObject( vtkStatisticsAlgorithm::OUTPUT_MODEL ) );

  // Select columns of interest
  pos->AddColumn( columnNames[0] );
  pos->AddColumn( columnNames[1] );

  // Test (in parallel) with Learn, Derive, and Assess options turned on
  pos->SetLearnOption( true );
  pos->SetDeriveOption( true );
  pos->SetAssessOption( false );
  pos->SetTestOption( false );
  pos->SetQuantize( args->quantize );
  pos->SetMaximumHistogramSize( args->maxHistoSize );
  pos->Update();

  // Synchronize and stop clock
  com->Barrier();
  timer->StopTimer();

  if ( myRank == args->ioRank )
    {
    cout << "\n## Completed parallel calculation of order statistics (with assessment):\n"
         << "   Wall time: "
         << timer->GetElapsedTime()
         << " sec.\n";
    }

  // Now perform verifications
  unsigned nbq = outputModelDS->GetNumberOfBlocks() - 1;
  vtkTable* outputCard = vtkTable::SafeDownCast( outputModelDS->GetBlock( nbq - 1 ) );

  // Verify that all processes have the same grand total and histograms size
  if ( myRank == args->ioRank )
    {
    cout << "\n## Verifying that all processes have the same grand total and histograms size.\n";
    }

  // Gather all cardinalities
  int numProcs = controller->GetNumberOfProcesses();
  int card_l = outputCard->GetValueByName( 0, "Cardinality" ).ToInt();
  int* card_g = new int[numProcs];
  com->AllGather( &card_l,
                  card_g,
                  1 );

  // Known global cardinality
  int testIntValue = args->nVals * numProcs;

  // Verify histogram cardinalities for each variable
  for ( int i = 0; i < nVariables; ++ i )
    {
    if ( myRank == args->ioRank )
      {
      cout << "   "
           << columnNames[i]
           << ":\n";
      }  // if ( myRank == args->ioRank )

    vtkTable* outputHistogram = vtkTable::SafeDownCast( outputModelDS->GetBlock( i ) );
    // Print out and verify all cardinalities
    if ( myRank == args->ioRank )
      {
      for ( int p = 0; p < numProcs; ++ p )
        {
        cout << "     On process "
             << p
             << ", cardinality = "
             << card_g[p]
             << ", histogram size = "
             << outputHistogram->GetNumberOfRows()
             << "\n";
        
        if ( card_g[p] != testIntValue )
          {
          vtkGenericWarningMacro("Incorrect cardinality:"
                                 << card_g[p]
                                 << " <> "
                                 << testIntValue
                                 << ")");
          *(args->retVal) = 1;
          }
        } // p
      } // if ( myRank == args->ioRank )
    } // i

  // Print out and verify global extrema
  vtkTable* outputQuantiles = vtkTable::SafeDownCast( outputModelDS->GetBlock( nbq ) );
  if ( myRank == args->ioRank )
    {
    cout << "\n## Verifying that calculated global ranges are correct:\n";

    for ( int i = 0; i < nVariables; ++ i )
      {
      vtkVariant min_c = outputQuantiles->GetValueByName( 0,
                                                   columnNames[i] );
      
      vtkVariant max_c = outputQuantiles->GetValueByName( outputQuantiles->GetNumberOfRows() - 1 ,
                                                   columnNames[i] );
      
      cout << "   "
           << columnNames[i]
           << ": "
           << min_c
           << " to "
           << max_c
           << "\n";

      // Check minimum
      if ( min_c.IsString() )
        {
        c = static_cast<char>( min_g[i] );
        if ( min_c.ToString() != vtkStdString( &c, 1 ) )
          {
          vtkGenericWarningMacro("Incorrect minimum for variable "
                                 << columnNames[i]);
          *(args->retVal) = 1;
          }
        } // if ( min_c.IsString() )
      else
        {
        if ( min_c != min_g[i] )
          {
          vtkGenericWarningMacro("Incorrect minimum for variable "
                                 << columnNames[i]);
          *(args->retVal) = 1;
          }
        } // else
      
      // Check maximum
      if ( max_c.IsString() )
        {
        c = static_cast<char>( max_g[i] );
        if ( max_c.ToString() != vtkStdString( &c, 1 ) )
          {
          vtkGenericWarningMacro("Incorrect maximum for variable "
                                 << columnNames[i]);
          *(args->retVal) = 1;
          }
        }
      else
        {
        if ( max_c != max_g[i] )
          {
          vtkGenericWarningMacro("Incorrect maximum for variable "
                                 << columnNames[i]);
          *(args->retVal) = 1;
          } //  ( max_c.IsString() )
        } // else
      } // i
    } // if ( myRank == args->ioRank )

  // Clean up
  delete [] card_g;
  pos->Delete();
  inputData->Delete();
  timer->Delete();
}

//----------------------------------------------------------------------------
int main( int argc, char** argv )
{
  // **************************** MPI Initialization ***************************
  vtkMPIController* controller = vtkMPIController::New();
  controller->Initialize( &argc, &argv );

  // If an MPI controller was not created, terminate in error.
  if ( ! controller->IsA( "vtkMPIController" ) )
    {
    vtkGenericWarningMacro("Failed to initialize a MPI controller.");
    controller->Delete();
    return 1;
    }

  vtkMPICommunicator* com = vtkMPICommunicator::SafeDownCast( controller->GetCommunicator() );

  // ************************** Find an I/O node ********************************
  int* ioPtr;
  int ioRank;
  int flag;

  MPI_Attr_get( MPI_COMM_WORLD,
                MPI_IO,
                &ioPtr,
                &flag );

  if ( ( ! flag ) || ( *ioPtr == MPI_PROC_NULL ) )
    {
    // Getting MPI attributes did not return any I/O node found.
    ioRank = MPI_PROC_NULL;
    vtkGenericWarningMacro("No MPI I/O nodes found.");

    // As no I/O node was found, we need an unambiguous way to report the problem.
    // This is the only case when a testValue of -1 will be returned
    controller->Finalize();
    controller->Delete();

    return -1;
    }
  else
    {
    if ( *ioPtr == MPI_ANY_SOURCE )
      {
      // Anyone can do the I/O trick--just pick node 0.
      ioRank = 0;
      }
    else
      {
      // Only some nodes can do I/O. Make sure everyone agrees on the choice (min).
      com->AllReduce( ioPtr,
                      &ioRank,
                      1,
                      vtkCommunicator::MIN_OP );
      }
    }

  // **************************** Parse command line ***************************
  // Set default argument values
  int nVals = 100000;
  double stdev = 50.;
  bool quantize = false;
  int maxHistoSize = 500;

  // Initialize command line argument parser
  vtksys::CommandLineArguments clArgs;
  clArgs.Initialize( argc, argv );
  clArgs.StoreUnusedArguments( false );

  // Parse per-process cardinality of each pseudo-random sample
  clArgs.AddArgument("--n-per-proc",
                     vtksys::CommandLineArguments::SPACE_ARGUMENT,
                     &nVals, "Per-process cardinality of each pseudo-random sample");

  // Parse standard deviation of pseudo-random Gaussian sample
  clArgs.AddArgument("--std-dev",
                     vtksys::CommandLineArguments::SPACE_ARGUMENT,
                     &stdev, "Standard deviation of pseudo-random Gaussian sample");

  // Parse maximum histogram size
  clArgs.AddArgument("--max-histo-size",
                     vtksys::CommandLineArguments::SPACE_ARGUMENT,
                     &maxHistoSize, "Maximum histogram size (when re-quantizing is allowed)");

  // Parse whether quantization should be used (to reduce histogram size)
  clArgs.AddArgument("--quantize",
                     vtksys::CommandLineArguments::NO_ARGUMENT,
                     &quantize, "Allow re-quantizing");


  // If incorrect arguments were provided, provide some help and terminate in error.
  if ( ! clArgs.Parse() )
    {
    if ( com->GetLocalProcessId() == ioRank )
      {
      cerr << "Usage: " 
           << clArgs.GetHelp()
           << "\n";
      }

    controller->Finalize();
    controller->Delete();

    return 1;
    }

  // ************************** Initialize test *********************************
  if ( com->GetLocalProcessId() == ioRank )
    {
    cout << "\n# Process "
         << ioRank
         << " will be the I/O node.\n";
    }


  // Parameters for regression test.
  int testValue = 0;
  RandomOrderStatisticsArgs args;
  args.nVals = nVals;
  args.stdev = stdev;
  args.quantize = quantize;
  args.maxHistoSize = maxHistoSize;
  args.retVal = &testValue;
  args.ioRank = ioRank;

  // Check how many processes have been made available
  int numProcs = controller->GetNumberOfProcesses();
  if ( controller->GetLocalProcessId() == ioRank )
    {
    cout << "\n# Running test with "
         << numProcs
         << " processes and standard deviation = "
         << args.stdev
         << " for rounded Gaussian variable.\n";
    }

  // Execute the function named "process" on both processes
  controller->SetSingleMethod( RandomOrderStatistics, &args );
  controller->SingleMethodExecute();

  // Clean up and exit
  if ( com->GetLocalProcessId() == ioRank )
    {
    cout << "\n# Test completed.\n\n";
    }

  controller->Finalize();
  controller->Delete();

  return testValue;
}
