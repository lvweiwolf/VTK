/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkUnsignedIntArray.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 1993-2002 Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkUnsignedIntArray.h"
#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkUnsignedIntArray, "1.33");
vtkStandardNewMacro(vtkUnsignedIntArray);

// Instantiate object.
vtkUnsignedIntArray::vtkUnsignedIntArray(vtkIdType numComp)
{
  this->NumberOfComponents = (numComp < 1 ? 1 : numComp);
  this->Array = NULL;
  this->TupleSize = 3;
  this->Tuple = new float[this->TupleSize]; //used for conversion
  this->SaveUserArray = 0;
}

// Desctructor for the vtkUnsignedIntArray class
vtkUnsignedIntArray::~vtkUnsignedIntArray()
{
  if ((this->Array) && (!this->SaveUserArray))
    {
    delete [] this->Array;
    }
  delete [] this->Tuple;
}
// This method lets the user specify data to be held by the array.  The 
// array argument is a pointer to the data.  size is the size of 
// the array supplied by the user.  Set save to 1 to keep the class
// from deleting the array when it cleans up or reallocates memory.
// The class uses the actual array provided; it does not copy the data 
// from the suppled array.
void vtkUnsignedIntArray::SetArray(unsigned int* array, vtkIdType size,
                                   int save)
{
  if ((this->Array) && (!this->SaveUserArray))
    {
      vtkDebugMacro (<< "Deleting the array...");
      delete [] this->Array;
    }
  else 
    {
      vtkDebugMacro (<<"Warning, array not deleted, but will point to new array.");
    }

  vtkDebugMacro(<<"Setting array to: " << array);

  this->Array = array;
  this->Size = size;
  this->MaxId = size-1;
  this->SaveUserArray = save;
}

// Allocate memory for this array. Delete old storage only if necessary.
int vtkUnsignedIntArray::Allocate(const vtkIdType sz,
                                  const vtkIdType vtkNotUsed(ext))
{
  if ( sz > this->Size )
    {
    if ((this->Array) && (!this->SaveUserArray))
      {
      delete [] this->Array;
      }
    this->Size = ( sz > 0 ? sz : 1);
    if ( (this->Array = new unsigned int[this->Size]) == NULL )
      {
      return 0;
      }
    this->SaveUserArray = 0;
    }

  this->MaxId = -1;

  return 1;
}

// Release storage and reset array to initial state.
void vtkUnsignedIntArray::Initialize()
{
  if (( this->Array != NULL ) && (!this->SaveUserArray))
    {
    delete [] this->Array;
    }
  this->Array = NULL;
  this->Size = 0;
  this->MaxId = -1;
  this->SaveUserArray = 0;
}

// Deep copy of another unsigned int array.
void vtkUnsignedIntArray::DeepCopy(vtkDataArray *sa)
{
  // Do nothing on a NULL input.
  if (sa == NULL)
    {
    return;
    }

  if ( sa->GetDataType() != VTK_UNSIGNED_INT )
    {
    vtkDataArray::DeepCopy(sa);
    return;
    }

  if ( this != sa )
    {
    if ((this->Array) && (!this->SaveUserArray))
      {
      delete [] this->Array;
      }

    this->NumberOfComponents = sa->GetNumberOfComponents();
    this->MaxId = sa->GetMaxId();
    this->Size = sa->GetSize();
    this->SaveUserArray = 0;

    this->Array = new unsigned int[this->Size];
    memcpy(this->Array, (unsigned int *)sa->GetVoidPointer(0),
           this->Size*sizeof(unsigned int));
    }
}

void vtkUnsignedIntArray::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  if (this->Array)
    {
    os << indent << "Array: " << this->Array << "\n";
    }
  else
    {
    os << indent << "Array: (null)\n";
    }
}

//
// Private function does "reallocate"
//
unsigned int *vtkUnsignedIntArray::ResizeAndExtend(const vtkIdType sz)
{
  unsigned int *newArray;
  vtkIdType newSize;

  if ( sz > this->Size ) 
    {
    newSize = this->Size + sz;
    }
  else if (sz == this->Size)
    {
    return this->Array;
    }
  else
    {
    newSize = sz;
    }

  if (newSize <= 0)
    {
    this->Initialize();
    return 0;
    }

  if ( (newArray = new unsigned int[newSize]) == NULL )
    {
    vtkErrorMacro(<< "Cannot allocate memory\n");
    return 0;
    }

  if (this->Array)
    {
    memcpy(newArray, this->Array, 
         (sz < this->Size ? sz : this->Size) * sizeof(unsigned int));
    if (!this->SaveUserArray)
      {
      delete[] this->Array;
      }
    }

  if (newSize < this->Size)
    {
    this->MaxId = newSize-1;
    }
  this->Size = newSize;
  this->Array = newArray;
  this->SaveUserArray = 0;

  return this->Array;
}

void vtkUnsignedIntArray::Resize(vtkIdType sz)
{
  unsigned int *newArray;
  vtkIdType newSize = sz*this->NumberOfComponents;

  if (newSize == this->Size)
    {
    return;
    }

  if (newSize <= 0)
    {
    this->Initialize();
    return;
    }

  if ( (newArray = new unsigned int[newSize]) == NULL )
    {
    vtkErrorMacro(<< "Cannot allocate memory\n");
    return;
    }

  if (this->Array)
    {
    memcpy(newArray, this->Array, 
         (newSize < this->Size ? newSize : this->Size) * sizeof(unsigned int));
    if (!this->SaveUserArray)
      {
      delete[] this->Array;
      }
    }

  if (newSize < this->Size)
    {
    this->MaxId = newSize-1;
    }
  this->Size = newSize;
  this->Array = newArray;
  this->SaveUserArray = 0;

  return;
}


// Set the number of n-tuples in the array.
void vtkUnsignedIntArray::SetNumberOfTuples(const vtkIdType number)
{
  this->SetNumberOfValues(number*this->NumberOfComponents);
}

// Get a pointer to a tuple at the ith location. This is a dangerous method
// (it is not thread safe since a pointer is returned).
float *vtkUnsignedIntArray::GetTuple(const vtkIdType i) 
{
  if ( this->TupleSize < this->NumberOfComponents )
    {
    this->TupleSize = this->NumberOfComponents;
    delete [] this->Tuple;
    this->Tuple = new float[this->TupleSize];
    }

  unsigned int *t = this->Array + this->NumberOfComponents*i;
  for (int j=0; j<this->NumberOfComponents; j++)
    {
    this->Tuple[j] = (float)t[j];
    }
  return this->Tuple;
}

// Copy the tuple value into a user-provided array.
void vtkUnsignedIntArray::GetTuple(const vtkIdType i, float * tuple) 
{
  unsigned int *t = this->Array + this->NumberOfComponents*i;
  for (int j=0; j<this->NumberOfComponents; j++)
    {
    tuple[j] = (float)t[j];
    }
}

void vtkUnsignedIntArray::GetTuple(const vtkIdType i, double * tuple) 
{
  unsigned int *t = this->Array + this->NumberOfComponents*i;
  for (int j=0; j<this->NumberOfComponents; j++)
    {
    tuple[j] = (double)t[j];
    }
}

// Set the tuple value at the ith location in the array.
void vtkUnsignedIntArray::SetTuple(const vtkIdType i, const float * tuple)
{
  vtkIdType loc = i * this->NumberOfComponents; 

  for (int j=0; j<this->NumberOfComponents; j++) 
    {
    this->Array[loc+j] = (unsigned int)tuple[j];
    }
}

void vtkUnsignedIntArray::SetTuple(const vtkIdType i, const double * tuple)
{
  vtkIdType loc = i * this->NumberOfComponents; 

  for (int j=0; j<this->NumberOfComponents; j++) 
    {
    this->Array[loc+j] = (unsigned int)tuple[j];
    }
}

// Insert (memory allocation performed) the tuple into the ith location
// in the array.
void vtkUnsignedIntArray::InsertTuple(const vtkIdType i, const float * tuple)
{
  unsigned int *t = this->WritePointer(i*this->NumberOfComponents,this->NumberOfComponents);

  for (int j=0; j<this->NumberOfComponents; j++)
    {
    *t++ = (unsigned int)*tuple++;
    }
}

void vtkUnsignedIntArray::InsertTuple(const vtkIdType i, const double * tuple)
{
  unsigned int *t = this->WritePointer(i*this->NumberOfComponents,this->NumberOfComponents);

  for (int j=0; j<this->NumberOfComponents; j++)
    {
    *t++ = (unsigned int)*tuple++;
    }
}

// Insert (memory allocation performed) the tuple onto the end of the array.
vtkIdType vtkUnsignedIntArray::InsertNextTuple(const float * tuple)
{
  vtkIdType i = this->MaxId + 1;
  unsigned int *t = this->WritePointer(i,this->NumberOfComponents);

  for (i=0; i<this->NumberOfComponents; i++)
    {
    *t++ = (unsigned int)*tuple++;
    }

  return this->MaxId / this->NumberOfComponents;
}

vtkIdType vtkUnsignedIntArray::InsertNextTuple(const double * tuple)
{
  vtkIdType i = this->MaxId + 1;
  unsigned int *t = this->WritePointer(i,this->NumberOfComponents);

  for (i=0; i<this->NumberOfComponents; i++)
    {
    *t++ = (unsigned int)*tuple++;
    }

  return this->MaxId / this->NumberOfComponents;
}

// Return the data component at the ith tuple and jth component location.
// Note that i<NumberOfTuples and j<NumberOfComponents.
float vtkUnsignedIntArray::GetComponent(const vtkIdType i, const int j)
{
  return static_cast<float>(this->GetValue(i*this->NumberOfComponents + j));
}

// Set the data component at the ith tuple and jth component location.
// Note that i<NumberOfTuples and j<NumberOfComponents. Make sure enough
// memory has been allocated (use SetNumberOfTuples() and 
// SetNumberOfComponents()).
void vtkUnsignedIntArray::SetComponent(const vtkIdType i, const int j, 
                                       float c)
{
  this->SetValue(i*this->NumberOfComponents + j, static_cast<unsigned int>(c));
}

void vtkUnsignedIntArray::InsertComponent(const vtkIdType i, const int j, 
                                          float c)
{
  this->InsertValue(i*this->NumberOfComponents + j, 
                    static_cast<unsigned int>(c));
}
