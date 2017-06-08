/* Generated from orogen/lib/orogen/templates/tasks/Task.cpp */

#include "RMLPositionTask.hpp"
#include <base/Logging.hpp>

using namespace trajectory_generation;

RMLPositionTask::RMLPositionTask(std::string const& name)
    : RMLPositionTaskBase(name)
{
    rml_flags = new RMLPositionFlags();
}

RMLPositionTask::RMLPositionTask(std::string const& name, RTT::ExecutionEngine* engine)
    : RMLPositionTaskBase(name, engine)
{
    rml_flags = new RMLPositionFlags();
}

RMLPositionTask::~RMLPositionTask()
{
    delete rml_flags;
}

bool RMLPositionTask::configureHook()
{
    rml_input_parameters = new RMLPositionInputParameters(_motion_constraints.get().size());
    rml_output_parameters = new RMLPositionOutputParameters(_motion_constraints.get().size());

    current_sample.resize(_motion_constraints.get().size());
    current_sample.names = _motion_constraints.get().names;

    if (! RMLPositionTaskBase::configureHook())
        return false;

    return true;
}
bool RMLPositionTask::startHook()
{
    if (! RMLPositionTaskBase::startHook())
        return false;
    return true;
}

void RMLPositionTask::updateHook()
{
    RMLPositionTaskBase::updateHook();
}

void RMLPositionTask::errorHook()
{
    RMLPositionTaskBase::errorHook();
}

void RMLPositionTask::stopHook()
{
    RMLPositionTaskBase::stopHook();
}

void RMLPositionTask::cleanupHook()
{
    RMLPositionTaskBase::cleanupHook();

    delete rml_input_parameters;
    delete rml_output_parameters;
}

void RMLPositionTask::setMotionConstraints(const MotionConstraints &constraints, RMLInputParameters* new_input_parameters){

    for(size_t i = 0; i < constraints.size(); i++){

        const MotionConstraint &constraint = constraints[i];

        uint idx;
        try{
            idx = motion_constraints.mapNameToIndex(constraints.names[i]);
        }
        catch(Exception e){
            LOG_ERROR("Trying to set motion constraint with name %s, but this name has to been configured", constraints.names[i].c_str());
            throw std::invalid_argument("Invalid motion constraints");
        }

        // Check if constraints are ok, e.g. max.speed > 0 etc
        constraint.validate();

#ifdef USING_REFLEXXES_TYPE_IV
        rml_input_parameters->MaxPositionVector->VecData[idx] = constraint.max.position;
        rml_input_parameters->MinPositionVector->VecData[idx] = constraint.min.position;
#endif
        ((RMLPositionInputParameters*)rml_input_parameters)->MaxVelocityVector->VecData[idx] = constraint.max.speed;
        rml_input_parameters->MaxAccelerationVector->VecData[idx] = constraint.max.acceleration;
        rml_input_parameters->MaxJerkVector->VecData[idx] = constraint.max_jerk;
    }
}

RTT::FlowStatus RMLPositionTask::updateCurrentState(RMLInputParameters& new_input_parameters){

    RTT::FlowStatus fs = _current_state.readNewest(current_state);
    if(fs == RTT::NewData){

        for(size_t i = 0; i < motion_constraints.size(); i++)
        {
            size_t idx;
            try{
                idx = current_state.mapNameToIndex(motion_constraints.names[i]);
            }
            catch(std::exception e){
                LOG_ERROR("%s: Element %s has been configured in motion constraints, but is not available in joint state",
                          this->getName().c_str(), motion_constraints.names[i].c_str());
                throw e;
            }

            // Init with current joint position and zero velocity/acceleration, if RML has not yet been called
            const base::JointState &state = current_state[idx];
            if(base::isNaN<int>(rml_result_value)){

                if(state.hasPosition()){
                    new_input_parameters->CurrentPositionVector->VecData[i] = current_sample[i].position = state.position;
                    new_input_parameters->CurrentVelocityVector->VecData[i] = current_sample[i].speed = 0;
                    new_input_parameters->CurrentAccelerationVector->VecData[i] = current_sample[i].acceleration = 0;
                }
                else{
                    LOG_ERROR("Position of joint %s is NaN", motion_constraints.names[i].c_str());
                    throw std::invalid_argument("Invalid joint state");
                }
            }
        }
        _current_state.time = base::Time::now();
        _current_sample.write(current_sample);
    }
    return fs;
}

RTT::FlowStatus RMLPositionTask::updateTarget(RMLInputParameters& new_input_parameters){

}

ReflexxesResultValue RMLPositionTask::performOTG(const RMLInputParameters& new_input_parameters, const RMLFlags& flags, RMLOutputParameters* new_output_parameters){

}

void RMLPositionTask::writeSample(const RMLOutputParameters& new_output_paramameters){

}

void RMLPositionTask::printParams(){
    ((RMLPositionInputParameters*)rml_input_parameters)->Echo();
    ((RMLPositionOutputParameters*)rml_output_parameters)->Echo();
}

ReflexxesResultValue RMLPositionTask::performOTG(base::commands::Joints &current_command)
{
    int result = rml_api->RMLPosition( *(RMLPositionInputParameters*)rml_input_parameters,
                                       (RMLPositionOutputParameters*)rml_output_parameters,
                                       *(RMLPositionFlags*)rml_flags );

    // Always feed back the new state as the current state:
    *rml_input_parameters->CurrentPositionVector     = *rml_output_parameters->NewPositionVector;
    *rml_input_parameters->CurrentVelocityVector     = *rml_output_parameters->NewVelocityVector;
    *rml_input_parameters->CurrentAccelerationVector = *rml_output_parameters->NewAccelerationVector;

    current_command.resize(motion_constraints.size());
    current_command.names = motion_constraints.names;

    for(size_t i = 0; i < motion_constraints.size(); i++){
        current_command[i].position = current_sample[i].position = rml_output_parameters->NewPositionVector->VecData[i];
        current_command[i].speed = current_sample[i].speed = rml_output_parameters->NewVelocityVector->VecData[i];
        current_command[i].acceleration = current_sample[i].acceleration = rml_output_parameters->NewAccelerationVector->VecData[i];
    }

    return (ReflexxesResultValue)result;
}

void  RMLPositionTask::setJointState(const base::JointState& state, const size_t idx)
{
    // Don't do anything here, since in position-based RML, the new position vector will always be fed back as current position
}

void RMLPositionTask::setTarget(const base::JointState& cmd, const size_t idx)
{
    if(!cmd.hasPosition()){
        LOG_ERROR("Target position of element %i is invalid: %f", idx, cmd.position);
        throw std::invalid_argument("Invalid target position");
    }

    double pos = cmd.position;

#ifdef USING_REFLEXXES_TYPE_IV  // Crop at limits if and only if POSITIONAL_LIMITS_ACTIVELY_PREVENT is selected
    if(rml_flags->PositionalLimitsBehavior == RMLFlags::POSITIONAL_LIMITS_ACTIVELY_PREVENT){
        double max = rml_input_parameters->MaxPositionVector->VecData[idx];
        double min = rml_input_parameters->MinPositionVector->VecData[idx];
        pos = std::max(std::min(max, pos), min);
    }
#endif

    ((RMLPositionInputParameters*)rml_input_parameters)->TargetPositionVector->VecData[idx] = pos;

    if(cmd.hasSpeed())
        rml_input_parameters->TargetVelocityVector->VecData[idx] = cmd.speed;
    else
        rml_input_parameters->TargetVelocityVector->VecData[idx] = 0;
}

const ReflexxesInputParameters& RMLPositionTask::fromRMLTypes(const RMLInputParameters &in, ReflexxesInputParameters& out){
    RMLTask::fromRMLTypes(in, out);
    memcpy(out.max_velocity_vector.data(), ((RMLPositionInputParameters& )in).MaxVelocityVector->VecData, sizeof(double) * in.GetNumberOfDOFs());
    memcpy(out.target_position_vector.data(), ((RMLPositionInputParameters& )in).TargetPositionVector->VecData, sizeof(double) * in.GetNumberOfDOFs());
    return out;
}

const ReflexxesOutputParameters& RMLPositionTask::fromRMLTypes(const RMLOutputParameters &in, ReflexxesOutputParameters& out)
{
    RMLTask::fromRMLTypes(in, out);
#ifdef USING_REFLEXXES_TYPE_IV
    out.trajectory_exceeds_target_position = ((RMLPositionOutputParameters&)in).TrajectoryExceedsTargetPosition;
#endif
    return out;
}