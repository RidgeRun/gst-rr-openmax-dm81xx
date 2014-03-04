/*
 * Copyright (C) 2013 RidgeRun
 * Copyright (C) 2006-2009 Texas Instruments, Incorporated
 * Copyright (C) 2007-2009 Nokia Corporation.
 *
 * Author: Felipe Contreras <felipe.contreras@nokia.com>
 * Author: Michael Gruner <michael.gruner@ridgerun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "gstomxerror.h"

const gchar *
gst_omx_error_to_str (OMX_ERRORTYPE omx_error)
{
  switch (omx_error) {
    case OMX_ErrorNone:
      return "None";

    case OMX_ErrorInsufficientResources:
      return
          "There were insufficient resources to perform the requested operation";

    case OMX_ErrorUndefined:
      return "The cause of the error could not be determined";

    case OMX_ErrorInvalidComponentName:
      return "The component name string was not valid";

    case OMX_ErrorComponentNotFound:
      return "No component with the specified name string was found";

    case OMX_ErrorInvalidComponent:
      return "The component specified did not have an entry point";

    case OMX_ErrorBadParameter:
      return "One or more parameters were not valid";

    case OMX_ErrorNotImplemented:
      return "The requested function is not implemented";

    case OMX_ErrorUnderflow:
      return "The buffer was emptied before the next buffer was ready";

    case OMX_ErrorOverflow:
      return "The buffer was not available when it was needed";

    case OMX_ErrorHardware:
      return "The hardware failed to respond as expected";

    case OMX_ErrorInvalidState:
      return "The component is in invalid state";

    case OMX_ErrorStreamCorrupt:
      return "Stream is found to be corrupt";

    case OMX_ErrorPortsNotCompatible:
      return "Ports being connected are not compatible";

    case OMX_ErrorResourcesLost:
      return "Resources allocated to an idle component have been lost";

    case OMX_ErrorNoMore:
      return "No more indices can be enumerated";

    case OMX_ErrorVersionMismatch:
      return "The component detected a version mismatch";

    case OMX_ErrorNotReady:
      return "The component is not ready to return data at this time";

    case OMX_ErrorTimeout:
      return "There was a timeout that occurred";

    case OMX_ErrorSameState:
      return
          "This error occurs when trying to transition into the state you are already in";

    case OMX_ErrorResourcesPreempted:
      return
          "Resources allocated to an executing or paused component have been preempted";

    case OMX_ErrorPortUnresponsiveDuringAllocation:
      return
          "Waited an unusually long time for the supplier to allocate buffers";

    case OMX_ErrorPortUnresponsiveDuringDeallocation:
      return
          "Waited an unusually long time for the supplier to de-allocate buffers";

    case OMX_ErrorPortUnresponsiveDuringStop:
      return
          "Waited an unusually long time for the non-supplier to return a buffer during stop";

    case OMX_ErrorIncorrectStateTransition:
      return "Attempting a state transition that is not allowed";

    case OMX_ErrorIncorrectStateOperation:
      return
          "Attempting a command that is not allowed during the present state";

    case OMX_ErrorUnsupportedSetting:
      return
          "The values encapsulated in the parameter or config structure are not supported";

    case OMX_ErrorUnsupportedIndex:
      return
          "The parameter or config indicated by the given index is not supported";

    case OMX_ErrorBadPortIndex:
      return "The port index supplied is incorrect";

    case OMX_ErrorPortUnpopulated:
      return
          "The port has lost one or more of its buffers and it thus unpopulated";

    case OMX_ErrorComponentSuspended:
      return "Component suspended due to temporary loss of resources";

    case OMX_ErrorDynamicResourcesUnavailable:
      return
          "Component suspended due to an inability to acquire dynamic resources";

    case OMX_ErrorMbErrorsInFrame:
      return "Frame generated macroblock error";

    case OMX_ErrorFormatNotDetected:
      return "Cannot parse or determine the format of an input stream";

    case OMX_ErrorContentPipeOpenFailed:
      return "The content open operation failed";

    case OMX_ErrorContentPipeCreationFailed:
      return "The content creation operation failed";

    case OMX_ErrorSeperateTablesUsed:
      return "Separate table information is being used";

    case OMX_ErrorTunnelingUnsupported:
      return "Tunneling is unsupported by the component";

    default:
      return "Unknown error";
  }
}

const gchar *
gst_omx_cmd_to_str (OMX_COMMANDTYPE omx_cmd)
{
  switch (omx_cmd) {
    case OMX_CommandStateSet:
      return "Set state";
    case OMX_CommandFlush:
      return "Flush";
    case OMX_CommandPortDisable:
      return "Port disable";
    case OMX_CommandPortEnable:
      return "Port enable";
    case OMX_CommandMarkBuffer:
      return "Mark buffer";
    default:
      return "Unknown command";
  }
}

const gchar *
gst_omx_state_to_str (OMX_STATETYPE omx_state)
{
  switch (omx_state) {
    case OMX_StateInvalid:
      return "Invalid";
    case OMX_StateLoaded:
      return "Loaded";
    case OMX_StateIdle:
      return "Idle";
    case OMX_StateExecuting:
      return "Executing";
    case OMX_StatePause:
      return "Pause";
    case OMX_StateWaitForResources:
      return "WaitForResources";
    default:
      return "Unknown state";
  }
}
