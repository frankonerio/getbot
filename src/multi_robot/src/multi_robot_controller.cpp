
#include <plansys2_pddl_parser/Utils.h>

#include <memory>

#include "plansys2_msgs/msg/action_execution_info.hpp"
#include "plansys2_msgs/msg/plan.hpp"

#include "plansys2_domain_expert/DomainExpertClient.hpp"
#include "plansys2_executor/ExecutorClient.hpp"
#include "plansys2_planner/PlannerClient.hpp"
#include "plansys2_problem_expert/ProblemExpertClient.hpp"

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"

class MulltiRobot : public rclcpp::Node
{
public:
  MulltiRobot()
  : rclcpp::Node("multi_robot_controller")
  {
  }

  bool init()
  {
    domain_expert_ = std::make_shared<plansys2::DomainExpertClient>();
    planner_client_ = std::make_shared<plansys2::PlannerClient>();
    problem_expert_ = std::make_shared<plansys2::ProblemExpertClient>();
    executor_client_ = std::make_shared<plansys2::ExecutorClient>();

    init_knowledge();

    auto domain = domain_expert_->getDomain();
    auto problem = problem_expert_->getProblem();
    auto plan = planner_client_->getPlan(domain, problem);

    if (!plan.has_value()) {
      std::cout << "Could not find plan to reach goal " <<
        parser::pddl::toString(problem_expert_->getGoal()) << std::endl;
      return false;
    }

    if (!executor_client_->start_plan_execution(plan.value())) {
      RCLCPP_ERROR(get_logger(), "Error starting a new plan (first)");
    }

    return true;
  }

  void init_knowledge()
  {
    problem_expert_->addInstance(plansys2::Instance{"P6Building", "world"});

    problem_expert_->addInstance(plansys2::Instance{"robot_1", "robot"});
    problem_expert_->addInstance(plansys2::Instance{"robot_2", "robot"});
    problem_expert_->addInstance(plansys2::Instance{"robot_3", "robot"});

    problem_expert_->addInstance(plansys2::Instance{"red_balls", "balls"});
    problem_expert_->addInstance(plansys2::Instance{"blue_balls","balls"});
    problem_expert_->addInstance(plansys2::Instance{"white_boxes", "boxes"});
    /***
    problem_expert_->addInstance(plansys2::Instance{"entrance", "room"});
    problem_expert_->addInstance(plansys2::Instance{"first_floor", "room"});
    problem_expert_->addInstance(plansys2::Instance{"charging_room", "room"});

    problem_expert_->addPredicate(plansys2::Predicate("(connected charging_room first_floor)"));
    problem_expert_->addPredicate(plansys2::Predicate("(connected first_floor charging_room)"));
    
    ***/

    problem_expert_->addPredicate(plansys2::Predicate("(items_dropped robot_1 red_balls)"));
    problem_expert_->addPredicate(plansys2::Predicate("(items_dropped robot_2 blue_balls)"));
    problem_expert_->addPredicate(plansys2::Predicate("(items_dropped robot_3 white_boxes)"));

    problem_expert_->addFunction(plansys2::Function("= battery_level robot_1 20"));
    problem_expert_->addFunction(plansys2::Function("= battery_level robot_2 100"));
    problem_expert_->addFunction(plansys2::Function("= battery_level robot_3 100"));

    problem_expert_->addFunction(plansys2::Function("= detected_items red_balls 10"));
    problem_expert_->addFunction(plansys2::Function("= detected_items blue_balls 10"));
    problem_expert_->addFunction(plansys2::Function("= detected_items white_boxes 10"));

    problem_expert_->addFunction(plansys2::Function("= at_target_items red_balls 0"));
    problem_expert_->addFunction(plansys2::Function("= at_target_items blue_balls 0"));
    problem_expert_->addFunction(plansys2::Function("= at_target_items white_boxes 0"));

    problem_expert_->addFunction(plansys2::Function("= carried_items robot_1 0"));
    problem_expert_->addFunction(plansys2::Function("= carried_items robot_2 0"));
    problem_expert_->addFunction(plansys2::Function("= carried_items robot_3 0"));

    problem_expert_->addFunction(plansys2::Function("= at_target_items_goal red_balls 2"));
    problem_expert_->addFunction(plansys2::Function("= at_target_items_goal blue_balls 1"));
    problem_expert_->addFunction(plansys2::Function("= at_target_items_goal white_boxes 0"));
    

    
    problem_expert_->setGoal(
      plansys2::Goal(
        "(and (items_handeled robot_1 red_balls) (items_handeled robot_2 blue_balls) (items_handeled robot_3 white_boxes))"));
  }

  void step()
  {
    auto feedback = executor_client_->getFeedBack();

      for (const auto &action_feedback : feedback.action_execution_status)
      {

        RCLCPP_INFO_STREAM(get_logger(), "[" << action_feedback.action_full_name << " " << action_feedback.completion * 100.0 << "%]");
      }
      std::cout << std::endl;

    std::vector<plansys2::Function> functions = problem_expert_->getFunctions();
    for (const auto & function : functions){
      if (function.name == "battery_level" && function.parameters[0].name == "robot_1"){
        if(function.value < battery_level_low){
          RCLCPP_INFO(get_logger(), "********************** BATTERY LOW ROBOT_1 *******************");
          problem_expert_->updateFunction(plansys2::Function("= battery_level robot_1 100"));

        }
      }
      if (function.name == "battery_level" && function.parameters[0].name == "robot_2"){
        if(function.value < battery_level_low){
          RCLCPP_INFO(get_logger(), "********************** BATTERY LOW ROBOT_2 *******************");
        }
      }
      if (function.name == "battery_level" && function.parameters[0].name == "robot_3"){
        if(function.value < battery_level_low){
          RCLCPP_INFO(get_logger(), "********************** BATTERY LOW ROBOT_3 *******************");

          problem_expert_->removePredicate(plansys2::Predicate("(items_handeled robot_3 white_boxes)"));
          problem_expert_->addPredicate(plansys2::Predicate("(battery_low robot_3)"));
          problem_expert_->addPredicate(plansys2::Predicate("(robot_at robot_3 entrance)"));
          problem_expert_->addPredicate(plansys2::Predicate("(charging_point_at charging_room)"));

          problem_expert_->setGoal(plansys2::Goal("(and(battery_full robot_3))"));

          auto domain = domain_expert_->getDomain();
          auto problem = problem_expert_->getProblem();
          auto plan = planner_client_->getPlan(domain, problem);

          if (plan.has_value())
          {
            std::cout << "Plan Found to Reach Goal: " << parser::pddl::toString(problem_expert_->getGoal()) << std::endl;

            executor_client_->execute_and_check_plan();
            std::cout << "EXECUTING PLAN " << std::endl;
            executor_client_->start_plan_execution(plan.value());
            break;
          }
          break;
        }
      }
        
    }

    if (!executor_client_->execute_and_check_plan()) {  // Plan finished
      auto result = executor_client_->getResult();

      if (result.value().success) {
        RCLCPP_INFO(get_logger(), "Plan succesfully finished");
      } else {
        RCLCPP_ERROR(get_logger(), "Plan finished with error");
      }
    }
  }

private:
  std::shared_ptr<plansys2::DomainExpertClient> domain_expert_;
  std::shared_ptr<plansys2::PlannerClient> planner_client_;
  std::shared_ptr<plansys2::ProblemExpertClient> problem_expert_;
  std::shared_ptr<plansys2::ExecutorClient> executor_client_;

  double battery_level_low = 40.0;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<MulltiRobot>();

  if (!node->init()) {
    return 0;
  }

  rclcpp::Rate rate(5);
  while (rclcpp::ok()) {
    node->step();

    rate.sleep();
    rclcpp::spin_some(node->get_node_base_interface());
  }

  rclcpp::shutdown();

  return 0;
}
