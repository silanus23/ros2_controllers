// Copyright 2020 PAL Robotics SL.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"

#include "test_forward_command_controller.hpp"

#include "forward_command_controller/forward_command_controller.hpp"
#include "hardware_interface/loaned_command_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "lifecycle_msgs/msg/state.hpp"
#include "rclcpp/node.hpp"
#include "rclcpp/qos.hpp"
#include "rclcpp/subscription.hpp"
#include "rclcpp/utilities.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"

using hardware_interface::LoanedCommandInterface;
using testing::IsEmpty;
using testing::SizeIs;

void ForwardCommandControllerTest::SetUpTestCase() { rclcpp::init(0, nullptr); }

void ForwardCommandControllerTest::TearDownTestCase() { rclcpp::shutdown(); }

void ForwardCommandControllerTest::SetUp()
{
  // initialize controller
  controller_ = std::make_unique<FriendForwardCommandController>();
}

void ForwardCommandControllerTest::TearDown() { controller_.reset(nullptr); }

void ForwardCommandControllerTest::SetUpController()
{
  const auto result = controller_->init(
    "forward_command_controller", "", 0, "", controller_->define_custom_node_options());
  ASSERT_EQ(result, controller_interface::return_type::OK);

  std::vector<LoanedCommandInterface> command_ifs;
  command_ifs.emplace_back(joint_1_pos_cmd_);
  command_ifs.emplace_back(joint_2_pos_cmd_);
  command_ifs.emplace_back(joint_3_pos_cmd_);
  controller_->assign_interfaces(std::move(command_ifs), {});
  executor.add_node(controller_->get_node()->get_node_base_interface());
}

TEST_F(ForwardCommandControllerTest, JointsParameterNotSet)
{
  SetUpController();
  controller_->get_node()->set_parameter({"interface_name", ""});

  // configure failed, 'joints' parameter not set
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::ERROR);
}

TEST_F(ForwardCommandControllerTest, InterfaceParameterNotSet)
{
  SetUpController();
  controller_->get_node()->set_parameter({"joints", std::vector<std::string>()});

  // configure failed, 'interface_name' parameter not set
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::ERROR);
}

TEST_F(ForwardCommandControllerTest, JointsParameterIsEmpty)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", std::vector<std::string>()});
  controller_->get_node()->set_parameter({"interface_name", ""});

  // configure failed, 'joints' is empty
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::ERROR);
}

TEST_F(ForwardCommandControllerTest, InterfaceParameterEmpty)
{
  SetUpController();
  controller_->get_node()->set_parameter({"joints", std::vector<std::string>{"joint1", "joint2"}});
  controller_->get_node()->set_parameter({"interface_name", ""});

  // configure failed, 'interface_name' is empty
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::ERROR);
}

TEST_F(ForwardCommandControllerTest, ConfigureParamsSuccess)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", std::vector<std::string>{"joint1", "joint2"}});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  // configure successful
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);

  // check interface configuration
  auto cmd_if_conf = controller_->command_interface_configuration();
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
  ASSERT_THAT(cmd_if_conf.names, SizeIs(2lu));
  auto state_if_conf = controller_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, IsEmpty());
  ASSERT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::NONE);
}

TEST_F(ForwardCommandControllerTest, ActivateWithWrongJointsNamesFails)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", std::vector<std::string>{"joint1", "joint4"}});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  // activate failed, 'joint4' is not a valid joint name for the hardware
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);
  ASSERT_EQ(
    controller_->on_activate(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::ERROR);
}

TEST_F(ForwardCommandControllerTest, ActivateWithWrongInterfaceNameFails)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "acceleration"});

  // activate failed, 'acceleration' is not a registered interface for `joint1`
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);
  ASSERT_EQ(
    controller_->on_activate(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::ERROR);
}

TEST_F(ForwardCommandControllerTest, ActivateSuccess)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  // activate successful
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);

  // check interface configuration
  auto cmd_if_conf = controller_->command_interface_configuration();
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
  ASSERT_THAT(cmd_if_conf.names, SizeIs(joint_names_.size()));
  auto state_if_conf = controller_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, IsEmpty());
  ASSERT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::NONE);

  ASSERT_EQ(
    controller_->on_activate(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);

  // check interface configuration
  cmd_if_conf = controller_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, SizeIs(joint_names_.size()));
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
  state_if_conf = controller_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, IsEmpty());
  ASSERT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::NONE);
}

TEST_F(ForwardCommandControllerTest, CommandSuccessTest)
{
  SetUpController();

  // configure controller
  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);

  // update successful though no command has been send yet
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check joint commands are still the default ones
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);

  // send command
  forward_command_controller::CmdType command_msg;
  command_msg.data = {10.0, 20.0, 30.0};
  controller_->rt_command_.set(command_msg);

  // update successful, command received
  ASSERT_EQ(
    controller_->update(rclcpp::Time(100000000), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check joint commands have been modified
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 10.0);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 20.0);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 30.0);
}

TEST_F(ForwardCommandControllerTest, WrongCommandCheckTest)
{
  SetUpController();

  // configure controller
  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);

  // send command with wrong number of joints
  forward_command_controller::CmdType command_msg;
  command_msg.data = {10.0, 20.0};
  controller_->rt_command_.set(command_msg);

  // update failed, command size does not match number of joints
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::ERROR);

  // check joint commands are still the default ones
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);
}

TEST_F(ForwardCommandControllerTest, NoCommandCheckTest)
{
  SetUpController();

  // configure controller
  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});
  ASSERT_EQ(
    controller_->on_configure(rclcpp_lifecycle::State()),
    controller_interface::CallbackReturn::SUCCESS);

  // update successful, no command received yet
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check joint commands are still the default ones
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);
}

TEST_F(ForwardCommandControllerTest, CommandCallbackTest)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  // default values
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);

  auto node_state = controller_->configure();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);

  node_state = controller_->get_node()->activate();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);

  // send a new command
  rclcpp::Node test_node("test_node");
  auto command_pub = test_node.create_publisher<std_msgs::msg::Float64MultiArray>(
    std::string(controller_->get_node()->get_name()) + "/commands", rclcpp::SystemDefaultsQoS());
  std_msgs::msg::Float64MultiArray command_msg;
  command_msg.data = {10.0, 20.0, 30.0};
  command_pub->publish(command_msg);

  // wait for command message to be passed
  const auto timeout = std::chrono::milliseconds{10};
  const auto until = controller_->get_node()->get_clock()->now() + timeout;
  while (controller_->get_node()->get_clock()->now() < until)
  {
    executor.spin_some();
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  // update successful
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check command in handle was set
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 10.0);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 20.0);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 30.0);
}

TEST_F(ForwardCommandControllerTest, DropInfiniteCommandCallbackTest)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  // default values
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);

  auto node_state = controller_->configure();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);

  node_state = controller_->get_node()->activate();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);

  // send a new command
  rclcpp::Node test_node("test_node");
  auto command_pub = test_node.create_publisher<std_msgs::msg::Float64MultiArray>(
    std::string(controller_->get_node()->get_name()) + "/commands", rclcpp::SystemDefaultsQoS());
  std_msgs::msg::Float64MultiArray command_msg;
  command_msg.data = {10.0, std::numeric_limits<double>::infinity(), 30.0};
  command_pub->publish(command_msg);

  // wait for command message to be passed
  const auto timeout = std::chrono::milliseconds{10};
  const auto until = controller_->get_node()->get_clock()->now() + timeout;
  while (controller_->get_node()->get_clock()->now() < until)
  {
    executor.spin_some();
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  // update successful
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check message containing infinite command value was rejected
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);
}

TEST_F(ForwardCommandControllerTest, ActivateDeactivateCommandsResetSuccess)
{
  SetUpController();

  controller_->get_node()->set_parameter({"joints", joint_names_});
  controller_->get_node()->set_parameter({"interface_name", "position"});

  // default values
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 1.1);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 2.1);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 3.1);

  auto node_state = controller_->configure();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);

  // check interface configuration
  auto cmd_if_conf = controller_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, SizeIs(joint_names_.size()));
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
  auto state_if_conf = controller_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, IsEmpty());
  EXPECT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::NONE);

  node_state = controller_->get_node()->activate();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);

  // check interface configuration
  cmd_if_conf = controller_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, SizeIs(joint_names_.size()));
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
  state_if_conf = controller_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, IsEmpty());
  EXPECT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::NONE);

  // send command
  forward_command_controller::CmdType command;
  command.data = {10.0, 20.0, 30.0};
  controller_->rt_command_.set(command);

  // update successful
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check command in handle was set
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 10);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 20);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 30);

  node_state = controller_->get_node()->deactivate();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE);

  // check interface configuration
  cmd_if_conf = controller_->command_interface_configuration();
  ASSERT_THAT(cmd_if_conf.names, SizeIs(joint_names_.size()));  // did not change
  EXPECT_EQ(cmd_if_conf.type, controller_interface::interface_configuration_type::INDIVIDUAL);
  state_if_conf = controller_->state_interface_configuration();
  ASSERT_THAT(state_if_conf.names, IsEmpty());
  EXPECT_EQ(state_if_conf.type, controller_interface::interface_configuration_type::NONE);

  // command ptr should be reset (nullptr) after deactivation - same check as in `update`
  auto cmd = controller_->rt_command_.get();
  ASSERT_THAT(
    cmd.data,
    ::testing::Each(::testing::NanSensitiveDoubleEq(std::numeric_limits<double>::quiet_NaN())));

  // Controller is inactive but let's put some data into buffer (simulate callback when inactive)
  command.data = {5.5, 6.6, 7.7};
  controller_->rt_command_.set(command);

  // command ptr should be available and message should be there - same check as in `update`
  cmd = controller_->rt_command_.get();
  ASSERT_THAT(
    cmd.data,
    ::testing::Each(
      ::testing::Not(::testing::NanSensitiveDoubleEq(std::numeric_limits<double>::quiet_NaN()))));

  // Now activate again
  node_state = controller_->get_node()->activate();
  ASSERT_EQ(node_state.id(), lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE);

  // command ptr should be reset after activation - same check as in `update`
  cmd = controller_->rt_command_.get();
  ASSERT_THAT(
    cmd.data,
    ::testing::Each(::testing::NanSensitiveDoubleEq(std::numeric_limits<double>::quiet_NaN())));

  // update successful
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // values should not change
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 10);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 20);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 30);

  // set commands again
  controller_->rt_command_.set(command);

  // update successful
  ASSERT_EQ(
    controller_->update(rclcpp::Time(0), rclcpp::Duration::from_seconds(0.01)),
    controller_interface::return_type::OK);

  // check command in handle was set
  ASSERT_EQ(joint_1_pos_cmd_.get_value(), 5.5);
  ASSERT_EQ(joint_2_pos_cmd_.get_value(), 6.6);
  ASSERT_EQ(joint_3_pos_cmd_.get_value(), 7.7);
}
