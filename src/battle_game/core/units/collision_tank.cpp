#include "battle_game/core/bullets/bullets.h"
#include "battle_game/core/game_core.h"
#include "battle_game/graphics/graphics.h"
#include "collision_tank.h"
#include <map>
#include <cmath>

namespace battle_game::unit {

namespace {
uint32_t tank_body_model_index = 0xffffffffu;
uint32_t tank_turret_model_index = 0xffffffffu;
}  // namespace

CollisionTank::CollisionTank(GameCore *game_core, uint32_t id, uint32_t player_id)
    : Unit(game_core, id, player_id) {
  if (!~tank_body_model_index) {
    auto mgr = AssetsManager::GetInstance();
    {
      /* Tank Body */
      tank_body_model_index = mgr->RegisterModel(
          {
              {{-0.8f, 0.8f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{-0.8f, -1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{0.8f, 0.8f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{0.8f, -1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              // distinguish front and back
              {{0.6f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
              {{-0.6f, 1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
          },
          {0, 1, 2, 1, 2, 3, 0, 2, 5, 2, 4, 5});
    }

    {
      /* Tank Turret */
      std::vector<ObjectVertex> turret_vertices;
      std::vector<uint32_t> turret_indices;
      const int precision = 60;
      const float inv_precision = 1.0f / float(precision);
      for (int i = 0; i < precision; i++) {
        auto theta = (float(i) + 0.5f) * inv_precision;
        theta *= glm::pi<float>() * 2.0f;
        auto sin_theta = std::sin(theta);
        auto cos_theta = std::cos(theta);
        turret_vertices.push_back({{sin_theta * 0.5f, cos_theta * 0.5f},
                                   {0.0f, 0.0f},
                                   {0.7f, 0.7f, 0.7f, 1.0f}});
        turret_indices.push_back(i);
        turret_indices.push_back((i + 1) % precision);
        turret_indices.push_back(precision);
      }
      turret_vertices.push_back(
          {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{-0.1f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{0.1f, 0.0f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{-0.1f, 1.2f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_vertices.push_back(
          {{0.1f, 1.2f}, {0.0f, 0.0f}, {0.7f, 0.7f, 0.7f, 1.0f}});
      turret_indices.push_back(precision + 1 + 0);
      turret_indices.push_back(precision + 1 + 1);
      turret_indices.push_back(precision + 1 + 2);
      turret_indices.push_back(precision + 1 + 1);
      turret_indices.push_back(precision + 1 + 2);
      turret_indices.push_back(precision + 1 + 3);
      tank_turret_model_index =
          mgr->RegisterModel(turret_vertices, turret_indices);
    }
  }
}

void CollisionTank::Render() {
  battle_game::SetTransformation(position_, rotation_);
  battle_game::SetTexture(0);
  battle_game::SetColor(game_core_->GetPlayerColor(player_id_));
  battle_game::DrawModel(tank_body_model_index);
  battle_game::SetRotation(turret_rotation_);
  battle_game::DrawModel(tank_turret_model_index);
}

void CollisionTank::InitAlreadyHit() {
  auto player = game_core_->GetPlayer(player_id_);
  auto &input_data = player->GetInputData();
  auto &units = game_core_->GetUnits();
  for (auto &unit : units) {
    if (unit.first != player_id_) {
      alreadyhit[unit.first] = false;
    }
  }
}

void CollisionTank::Update() {
  TankMove(speed_, glm::radians(180.0f));
  AlreadyHit();
  TurretRotate();
  Fire();
}

void CollisionTank::TurretRotate() {
  auto player = game_core_->GetPlayer(player_id_);
  if (player) {
    auto &input_data = player->GetInputData();
    auto diff = input_data.mouse_cursor_position - position_;
    if (glm::length(diff) < 1e-4) {
      turret_rotation_ = rotation_;
    } else {
      turret_rotation_ = std::atan2(diff.y, diff.x) - glm::radians(90.0f);
    }
  }
}

void CollisionTank::TankMove(float move_speed, float rotate_angular_speed) {
  auto player = game_core_->GetPlayer(player_id_);
  if (player) {
    auto &input_data = player->GetInputData();
    glm::vec2 offset{0.0f, 1.0f};

    if (input_data.key_down[GLFW_KEY_W]) {
      if (speed_ < 4.0f)
        speed_ += kSecondPerTick * acceleration_;  // ����ٶȽ�Сʹ�ú���ٶ�
      else
        speed_ += kSecondPerTick * power_ / speed_;  // �ٶȽϴ�ʹ�ú㹦��
    }
    if (input_data.key_down[GLFW_KEY_S]) {
      if (speed_ > -4.0f)
        speed_ -= kSecondPerTick * acceleration_;
      else
        speed_ += kSecondPerTick * power_ / speed_;
    }

    float speed = move_speed * GetSpeedScale();
    offset *= kSecondPerTick * speed;
    auto new_position =
        position_ + glm::vec2{glm::rotate(glm::mat4{1.0f}, rotation_,
                                          glm::vec3{0.0f, 0.0f, 1.0f}) *
                              glm::vec4{offset, 0.0f, 0.0f}};
    if (!game_core_->IsBlockedByObstacles(new_position)) {
      game_core_->PushEventMoveUnit(id_, new_position);
    } else {
      speed_ /= 1.05;
    }
    float rotation_offset = 0.0f;
    if (input_data.key_down[GLFW_KEY_A]) {
      rotation_offset += 1.0f;
    }
    if (input_data.key_down[GLFW_KEY_D]) {
      rotation_offset -= 1.0f;
    }
    rotation_offset *= kSecondPerTick * rotate_angular_speed * GetSpeedScale();
    game_core_->PushEventRotateUnit(id_, rotation_ + rotation_offset);

    if (input_data.key_down[GLFW_KEY_Q]) {
      friction_acceleration_ = 10.0f;  // �ƶ�ʱĦ���������
    } else {
      friction_acceleration_ = 2.5f;
    }

    if (speed_ < 0) {
      speed_ += kSecondPerTick * friction_acceleration_;
      speed_ = speed_ > 0 ? 0 : speed_;
    } else {
      speed_ -= kSecondPerTick * friction_acceleration_;
      speed_ = speed_ < 0 ? 0 : speed_;
    }  // Ħ����
  }
}

void CollisionTank::AlreadyHit() {
  auto player = game_core_->GetPlayer(player_id_);
  auto &input_data = player->GetInputData();
  auto &units = game_core_->GetUnits();
  for (auto &unit : units) {
    if(unit.first!=player_id_) {
      alreadyhit[unit.first] =
          ((alreadyhit[unit.first] == true) && (unit.second->IsHit(position_) == true))
              ? true
              : false;
    }
  }
}

void CollisionTank::Fire() {
  if (fire_count_down_) {
    fire_count_down_--;
  } 
  else {
    auto player = game_core_->GetPlayer(player_id_);
    if (player) {
      auto &input_data = player->GetInputData();
      auto &units = game_core_->GetUnits();
      for (auto &unit : units) {
        if (unit.first == player_id_) {
          continue;
        }
        if ((unit.first != id_) && (unit.second->IsHit(position_)) &&
            (alreadyhit[unit.first]==false)) {
          game_core_->PushEventDealDamage(unit.first, id_, 20.0f * std::abs(speed_));
          game_core_->PushEventDealDamage(id_, unit.first, 5.0f * std::abs(speed_));
          alreadyhit[unit.first] = true;
        }
      }
      fire_count_down_ = kTickPerSecond;  // Fire interval 1 second.
    }
  }
}

bool CollisionTank::IsHit(glm::vec2 position) const {
  position = WorldToLocal(position);
  return glm::length(position) < 1.0f;
}

const char *CollisionTank::UnitName() const {
  return "Collision Tank";
}

const char *CollisionTank::Author() const {
  return "Ljy and ZKX";
}
}  // namespace battle_game::unit