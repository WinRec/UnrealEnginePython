#include "UnrealEnginePythonPrivatePCH.h"
#include "PythonDelegate.h"

UPythonDelegate::UPythonDelegate() {
	py_callable = nullptr;
	signature_set = false;
}

void UPythonDelegate::SetPyCallable(PyObject *callable)
{
	// do not acquire the gil here as we set the callable in python call themselves
	py_callable = callable;
	Py_INCREF(py_callable);
}

void UPythonDelegate::SetSignature(UFunction *original_signature)
{
	signature = original_signature;
	signature_set = true;
}

void UPythonDelegate::ProcessEvent(UFunction *function, void *Parms)
{

	if (!py_callable)
		return;

	FScopePythonGIL gil;

	PyObject *py_args = nullptr;

	if (signature_set) {
		py_args = PyTuple_New(signature->NumParms);
		Py_ssize_t argn = 0;

		TFieldIterator<UProperty> PArgs(signature);
		for (; PArgs && argn < signature->NumParms && ((PArgs->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm); ++PArgs) {
			UProperty *prop = *PArgs;
			PyObject *arg = ue_py_convert_property(prop, (uint8 *)Parms);
			if (!arg) {
				unreal_engine_py_log_error();
				Py_DECREF(py_args);
				return;
			}
			PyTuple_SetItem(py_args, argn, arg);
			argn++;
		}
	}

	PyObject *ret = PyObject_CallObject(py_callable, py_args);
	Py_XDECREF(py_args);
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	// currently useless as events do not return a value
	/*
	if (signature_set) {
		UProperty *return_property = signature->GetReturnProperty();
		if (return_property && signature->ReturnValueOffset != MAX_uint16) {
			if (!ue_py_convert_pyobject(ret, return_property, (uint8 *)Parms)) {
				UE_LOG(LogPython, Error, TEXT("Invalid return value type for delegate"));
			}
		}
	}
	*/
	Py_DECREF(ret);
}

void UPythonDelegate::PyFakeCallable()
{
}

void UPythonDelegate::PyInputHandler()
{
	FScopePythonGIL gil;
	PyObject *ret = PyObject_CallObject(py_callable, NULL);
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);
}

void UPythonDelegate::PyInputAxisHandler(float value)
{
	FScopePythonGIL gil;
	PyObject *ret = PyObject_CallFunction(py_callable, (char *)"f", value);
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);
}

bool UPythonDelegate::Tick(float DeltaTime)
{
	FScopePythonGIL gil;
	PyObject *ret = PyObject_CallFunction(py_callable, (char *)"f", DeltaTime);
	if (!ret) {
		unreal_engine_py_log_error();
		return false;
	}
	if (PyObject_IsTrue(ret)) {
		Py_DECREF(ret);
		return true;
	}
	Py_DECREF(ret);
	return false;
}

#if WITH_EDITOR
void UPythonDelegate::PyFOnAssetPostImport(UFactory *factory, UObject *u_object)
{
	FScopePythonGIL gil;
	PyObject *ret = PyObject_CallFunction(py_callable, (char *)"OO", ue_get_python_wrapper((UObject *)factory), ue_get_python_wrapper(u_object));
	if (!ret) {
		unreal_engine_py_log_error();
		return;
	}
	Py_DECREF(ret);
}
#endif


UPythonDelegate::~UPythonDelegate()
{
	FScopePythonGIL gil;
	Py_XDECREF(py_callable);
#if defined(UEPY_MEMORY_DEBUG)
	UE_LOG(LogPython, Warning, TEXT("PythonDelegate callable XDECREF'ed"));
#endif
}
