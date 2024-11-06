import pygame
import sys
import math
import random
from enum import Enum
from dataclasses import dataclass
from typing import List, Tuple, Optional, Dict
from abc import ABC, abstractmethod
import time

class GameState(Enum):
    MENU = "menu"
    PLAYING = "playing"
    PAUSED = "paused"
    GAME_OVER = "game_over"

@dataclass
class GameConfig:
    SCREEN_WIDTH: int = 800
    SCREEN_HEIGHT: int = 600
    FPS: int = 60
    GRAVITY: float = 0.5
    TERRAIN_RESOLUTION: int = 2  # Pixels per terrain segment
    MIN_GROUND_HEIGHT: int = 100
    MAX_GROUND_HEIGHT: int = 200
    DESTRUCTION_FADE_SPEED: int = 5

class Colors:
    WHITE = (255, 255, 255)
    BLACK = (0, 0, 0)
    RED = (255, 0, 0)
    GREEN = (0, 255, 0)
    BLUE = (0, 0, 255)
    BROWN = (139, 69, 19)
    YELLOW = (255, 255, 0)

class Ability(ABC):
    def __init__(self, cooldown: float):
        self.cooldown = cooldown
        self.current_cooldown = 0

    @abstractmethod
    def activate(self, user, target) -> bool:
        pass

    def update(self, dt: float):
        if self.current_cooldown > 0:
            self.current_cooldown -= dt

class BiteAbility(Ability):
    def __init__(self):
        super().__init__(cooldown=2.0)
        self.damage = 20
        self.range = 30

    def activate(self, user, target) -> bool:
        if self.current_cooldown > 0:
            return False
        
        distance = math.hypot(user.x - target.x, user.y - target.y)
        if distance <= self.range:
            target.take_damage(self.damage)
            self.current_cooldown = self.cooldown
            return True
        return False

class AcidSprayAbility(Ability):
    def __init__(self):
        super().__init__(cooldown=5.0)
        self.damage = 15
        self.range = 50
        self.area_effect = 20

    def activate(self, user, target) -> bool:
        if self.current_cooldown > 0:
            return False
            
        distance = math.hypot(user.x - target.x, user.y - target.y)
        if distance <= self.range:
            target.take_damage(self.damage)
            target.apply_effect("acid", duration=3.0)
            self.current_cooldown = self.cooldown
            return True
        return False

class StatusEffect:
    def __init__(self, duration: float):
        self.duration = duration
        self.remaining_time = duration

    def update(self, dt: float, target) -> bool:
        self.remaining_time -= dt
        return self.remaining_time > 0

class AcidEffect(StatusEffect):
    def __init__(self, duration: float):
        super().__init__(duration)
        self.damage_per_second = 5

    def update(self, dt: float, target) -> bool:
        target.take_damage(self.damage_per_second * dt)
        return super().update(dt, target)

class Terrain:
    def __init__(self, config: GameConfig):
        self.config = config
        self.surface = pygame.Surface((config.SCREEN_WIDTH, config.SCREEN_HEIGHT), pygame.SRCALPHA)
        self.height_map = self.generate_terrain()
        self.destruction_map = pygame.Surface((config.SCREEN_WIDTH, config.SCREEN_HEIGHT), pygame.SRCALPHA)
        self.render_terrain()

    def generate_terrain(self) -> List[int]:
        """Generates terrain using Perlin-like noise"""
        heights = []
        phase = random.uniform(0, math.pi * 2)
        
        # Combine multiple sine waves for more natural-looking terrain
        for x in range(0, self.config.SCREEN_WIDTH, self.config.TERRAIN_RESOLUTION):
            height = self.config.MIN_GROUND_HEIGHT
            height += math.sin(x * 0.02 + phase) * 30
            height += math.sin(x * 0.05 + phase * 2) * 15
            height += math.sin(x * 0.01 + phase * 0.5) * 45
            height = min(max(height, self.config.MIN_GROUND_HEIGHT), self.config.MAX_GROUND_HEIGHT)
            heights.append(int(height))
        return heights

    def render_terrain(self):
        """Renders the terrain to the surface"""
        self.surface.fill((0, 0, 0, 0))
        
        # Create terrain polygon
        points = [(0, self.config.SCREEN_HEIGHT)]
        for x, height in enumerate(self.height_map):
            x = x * self.config.TERRAIN_RESOLUTION
            y = self.config.SCREEN_HEIGHT - height
            points.append((x, y))
        points.append((self.config.SCREEN_WIDTH, self.config.SCREEN_HEIGHT))
        
        # Draw terrain with gradient
        pygame.draw.polygon(self.surface, Colors.GREEN, points)
        
        # Add texture/detail
        for i in range(len(points) - 2):
            x = points[i][0]
            y = points[i][1]
            if random.random() < 0.1:  # 10% chance for each segment
                pygame.draw.circle(self.surface, (0, 200, 0), (int(x), int(y)), 2)

    def apply_destruction(self, x: int, y: int, radius: int):
        """Applies destruction effect at the given point"""
        # Create destruction circle
        pygame.draw.circle(self.destruction_map, (0, 0, 0, 255), (x, y), radius)
        
        # Update height map around the impact
        impact_start = max(0, x // self.config.TERRAIN_RESOLUTION - radius // self.config.TERRAIN_RESOLUTION)
        impact_end = min(len(self.height_map),
                        (x + radius) // self.config.TERRAIN_RESOLUTION + 1)
        
        for i in range(impact_start, impact_end):
            dist = abs(x - i * self.config.TERRAIN_RESOLUTION)
            if dist < radius:
                height_reduction = int((radius - dist) * 0.5)
                self.height_map[i] = max(self.config.MIN_GROUND_HEIGHT,
                                       self.height_map[i] - height_reduction)
        
        self.render_terrain()

    def get_height_at(self, x: int) -> int:
        """Returns the terrain height at the given x coordinate"""
        if x < 0 or x >= self.config.SCREEN_WIDTH:
            return self.config.MIN_GROUND_HEIGHT
            
        index = x // self.config.TERRAIN_RESOLUTION
        index = min(len(self.height_map) - 1, index)
        return self.height_map[index]

    def check_collision(self, x: int, y: int) -> bool:
        """Checks if a point collides with the terrain"""
        if x < 0 or x >= self.config.SCREEN_WIDTH or y < 0 or y >= self.config.SCREEN_HEIGHT:
            return False
            
        terrain_height = self.config.SCREEN_HEIGHT - self.get_height_at(x)
        return y >= terrain_height

class Projectile:
    def __init__(self, x: float, y: float, velocity: List[float], damage: int = 30):
        self.x = x
        self.y = y
        self.velocity = velocity
        self.damage = damage
        self.active = True
        self.trail: List[Tuple[float, float]] = []
        self.trail_length = 10

    def update(self, config: GameConfig):
        """Updates projectile position and trail"""
        # Add current position to trail
        self.trail.append((self.x, self.y))
        if len(self.trail) > self.trail_length:
            self.trail.pop(0)

        # Update position
        self.velocity[1] += config.GRAVITY
        self.x += self.velocity[0]
        self.y += self.velocity[1]

    def draw(self, screen: pygame.Surface):
        """Draws the projectile and its trail"""
        if not self.active:
            return

        # Draw trail
        if len(self.trail) > 1:
            for i in range(len(self.trail) - 1):
                alpha = int(255 * (i / len(self.trail)))
                pygame.draw.line(screen, (*Colors.YELLOW, alpha),
                               (int(self.trail[i][0]), int(self.trail[i][1])),
                               (int(self.trail[i+1][0]), int(self.trail[i+1][1])), 2)

        # Draw projectile
        pygame.draw.circle(screen, Colors.RED, (int(self.x), int(self.y)), 4)

class Ant:
    def __init__(self, x: float, y: float, color: Tuple[int, int, int] = Colors.BLACK, is_ai: bool = False):
        self.x = x
        self.y = y
        self.color = color
        self.health = 100
        self.max_health = 100
        self.is_alive = True
        self.is_ai = is_ai
        self.velocity = [0, 0]
        self.direction = 1  # 1 for right, -1 for left
        
        # Animation states
        self.walking = False
        self.animation_frame = 0
        self.animation_timer = 0
        
        # Abilities
        self.abilities: Dict[str, Ability] = {
            'bite': BiteAbility(),
            'acid_spray': AcidSprayAbility()
        }
        
        # Status effects
        self.active_effects: List[StatusEffect] = []

    def update(self, dt: float, terrain: Terrain):
        """Updates ant state"""
        if not self.is_alive:
            return

        # Update position and apply gravity
        self.x += self.velocity[0]
        self.y += self.velocity[1]
        
        # Ground collision
        ground_height = terrain.get_height_at(int(self.x))
        if self.y > terrain.config.SCREEN_HEIGHT - ground_height:
            self.y = terrain.config.SCREEN_HEIGHT - ground_height
            self.velocity[1] = 0
        else:
            self.velocity[1] += terrain.config.GRAVITY

        # Update abilities
        for ability in self.abilities.values():
            ability.update(dt)

        # Update status effects
        self.active_effects = [effect for effect in self.active_effects 
                             if effect.update(dt, self)]

        # Update animation
        if self.walking:
            self.animation_timer += dt
            if self.animation_timer >= 0.1:  # Animation frame rate
                self.animation_timer = 0
                self.animation_frame = (self.animation_frame + 1) % 4

    def draw(self, screen: pygame.Surface):
        """Draws the ant with animations and effects"""
        if not self.is_alive:
            return

        # Draw ant body
        body_points = self.get_body_points()
        pygame.draw.polygon(screen, self.color, body_points)
        
        # Draw legs with animation
        self.draw_legs(screen)
        
        # Draw antennae
        self.draw_antennae(screen)
        
        # Draw eyes
        eye_color = Colors.RED if any(isinstance(effect, AcidEffect) 
                                    for effect in self.active_effects) else Colors.WHITE
        self.draw_eyes(screen, eye_color)
        
        # Draw health bar
        self.draw_health_bar(screen)
        
        # Draw ability cooldowns
        self.draw_ability_cooldowns(screen)

    def get_body_points(self) -> List[Tuple[int, int]]:
        """Returns points for drawing ant body"""
        width = 20
        height = 10
        
        points = [
            (self.x - width//2, self.y - height//2),
            (self.x + width//2, self.y - height//2),
            (self.x + width//2, self.y + height//2),
            (self.x - width//2, self.y + height//2)
        ]
        
        return points

    def draw_legs(self, screen: pygame.Surface):
        """Draws ant legs with walking animation"""
        leg_pairs = 3
        leg_length = 8
        body_width = 20
        
        for i in range(leg_pairs):
            # Calculate leg positions with animation
            angle_offset = math.sin(self.animation_frame * 0.5 + i) * 0.3
            
            # Left leg
            start_x = self.x - body_width//2 + (i * body_width//(leg_pairs-1))
            pygame.draw.line(screen, self.color,
                           (start_x, self.y),
                           (start_x - leg_length * math.cos(angle_offset),
                            self.y + leg_length * math.sin(angle_offset)), 2)
            
            # Right leg
            pygame.draw.line(screen, self.color,
                           (start_x, self.y),
                           (start_x + leg_length * math.cos(angle_offset),
                            self.y + leg_length * math.sin(angle_offset)), 2)

    def draw_antennae(self, screen: pygame.Surface):
        """Draws ant antennae"""
        antenna_length = 12
        antenna_segments = 3
        
        for i in range(2):
            start_x = self.x - 5 + i * 10
            current_x = start_x
            current_y = self.y - 5
            
            for j in range(antenna_segments):
                angle = math.sin(self.animation_frame * 0.2 + i) * 0.3
                end_x = current_x + antenna_length/antenna_segments * math.cos(angle)
                end_y = current_y - antenna_length/antenna_segments * math.sin(angle)
                
                pygame.draw.line(screen, self.color,
                               (current_x, current_y),
                               (end_x, end_y), 1)
                
                current_x = end_x
                current_y = end_y

    def draw_eyes(self, screen: pygame.Surface, color: Tuple[int, int, int]):
        """Draws ant eyes"""
        eye_radius = 2
        eye_spacing = 6
        
        pygame.draw.circle(screen, color,
                         (int(self.x - eye_spacing//2), int(self.y - 2)),
                         eye_radius)
        pygame.draw.circle(screen, color,
                         (int(self.x + eye_spacing//2), int(self.y - 2)),
                         eye_radius)
        
    def draw_health_bar(self, screen: pygame.Surface):
        """Draws health bar above ant"""
        bar_width = 30
        bar_height = 4
        bar_pos = (int(self.x - bar_width/2), int(self.y - 20))
        
        # Background (red)
        pygame.draw.rect(screen, Colors.RED,
                        (*bar_pos, bar_width, bar_height))
        # Health (green)
        health_width = bar_width * (self.health / self.max_health)
        pygame.draw.rect(screen, Colors.GREEN,
                        (*bar_pos, health_width, bar_height))

    def draw_ability_cooldowns(self, screen: pygame.Surface):
        """Draws cooldown indicators for abilities"""
        indicator_size = 5
        spacing = 8
        y_pos = self.y + 15
        
        for i, (name, ability) in enumerate(self.abilities.items()):
            x_pos = self.x - len(self.abilities) * spacing/2 + i * spacing
            color = Colors.GREEN if ability.current_cooldown <= 0 else Colors.RED
            pygame.draw.circle(screen, color, (int(x_pos), int(y_pos)), indicator_size)

    def take_damage(self, amount: int):
        """Handles damage taken by the ant"""
        if not self.is_alive:
            return
            
        self.health = max(0, self.health - amount)
        if self.health <= 0:
            self.is_alive = False

    def apply_effect(self, effect_type: str, duration: float):
        """Applies a status effect to the ant"""
        if effect_type == "acid":
            self.active_effects.append(AcidEffect(duration))

    def use_ability(self, ability_name: str, target) -> bool:
        """Attempts to use the named ability"""
        if ability_name in self.abilities:
            return self.abilities[ability_name].activate(self, target)
        return False

class AIController:
    def __init__(self, ant: Ant, config: GameConfig):
        self.ant = ant
        self.config = config
        self.state = "idle"
        self.state_timer = 0
        self.target = None
        self.difficulty = 0.7  # 0 to 1, higher is more difficult

    def update(self, dt: float, player_ant: Ant, terrain: Terrain):
        """Updates AI behavior"""
        if not self.ant.is_alive:
            return

        self.state_timer -= dt
        distance_to_player = abs(self.ant.x - player_ant.x)

        # State machine for AI behavior
        if self.state == "idle":
            if self.state_timer <= 0:
                if distance_to_player < 100:
                    self.state = "attack"
                    self.state_timer = 2.0
                else:
                    self.state = "move"
                    self.state_timer = random.uniform(1.0, 3.0)

        elif self.state == "move":
            if self.state_timer <= 0 or distance_to_player < 50:
                self.state = "idle"
                self.state_timer = random.uniform(0.5, 1.5)
            else:
                # Move towards player with some randomness
                direction = 1 if player_ant.x > self.ant.x else -1
                if random.random() < 0.2:  # 20% chance to move randomly
                    direction *= -1
                self.ant.x += direction * 2

        elif self.state == "attack":
            if distance_to_player < 120 and random.random() < self.difficulty:
                # Choose and use an ability
                if distance_to_player < 40 and random.random() < 0.7:
                    self.ant.use_ability('bite', player_ant)
                else:
                    self.ant.use_ability('acid_spray', player_ant)
            
            self.state = "idle"
            self.state_timer = random.uniform(1.0, 2.0)

class Game:
    def __init__(self):
        pygame.init()
        self.config = GameConfig()
        self.screen = pygame.display.set_mode((self.config.SCREEN_WIDTH, self.config.SCREEN_HEIGHT))
        pygame.display.set_caption("Enhanced Ant Battle")
        
        self.clock = pygame.time.Clock()
        self.state = GameState.MENU
        self.terrain = Terrain(self.config)
        
        # Game objects
        self.player = Ant(100, self.config.SCREEN_HEIGHT - 150)
        self.enemy = Ant(self.config.SCREEN_WIDTH - 150, 
                        self.config.SCREEN_HEIGHT - 150,
                        Colors.BLUE, True)
        self.ai_controller = AIController(self.enemy, self.config)
        
        self.projectile = None
        self.charging_power = 0
        
        # UI elements
        self.font = pygame.font.Font(None, 36)

    def handle_menu(self):
        """Handles menu state"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_SPACE:
                    self.state = GameState.PLAYING
                elif event.key == pygame.K_ESCAPE:
                    return False
        
        # Draw menu
        self.screen.fill(Colors.WHITE)
        title = self.font.render("Ant Battle", True, Colors.BLACK)
        start_text = self.font.render("Press SPACE to Start", True, Colors.BLACK)
        controls_text = self.font.render("Z: Bite  X: Acid  SPACE: Shoot", True, Colors.BLACK)
        quit_text = self.font.render("Press ESC to Quit", True, Colors.BLACK)
        
        self.screen.blit(title, 
                        (self.config.SCREEN_WIDTH/2 - title.get_width()/2, 200))
        self.screen.blit(start_text,
                        (self.config.SCREEN_WIDTH/2 - start_text.get_width()/2, 300))
        self.screen.blit(controls_text,
                        (self.config.SCREEN_WIDTH/2 - controls_text.get_width()/2, 350))
        self.screen.blit(quit_text,
                        (self.config.SCREEN_WIDTH/2 - quit_text.get_width()/2, 400))
        
        pygame.display.flip()
        return True

    def handle_game_over(self):
        """Handles game over state"""
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_SPACE:
                    self.__init__()  # Reset game
                    return True
                elif event.key == pygame.K_ESCAPE:
                    return False
        
        # Draw game over screen
        self.screen.fill(Colors.WHITE)
        winner_text = "Player Wins!" if self.enemy.health <= 0 else "AI Wins!"
        text = self.font.render(winner_text, True, Colors.BLACK)
        restart_text = self.font.render("Press SPACE to Restart", True, Colors.BLACK)
        quit_text = self.font.render("Press ESC to Quit", True, Colors.BLACK)
        
        self.screen.blit(text,
                        (self.config.SCREEN_WIDTH/2 - text.get_width()/2, 250))
        self.screen.blit(restart_text,
                        (self.config.SCREEN_WIDTH/2 - restart_text.get_width()/2, 300))
        self.screen.blit(quit_text,
                        (self.config.SCREEN_WIDTH/2 - quit_text.get_width()/2, 350))
        
        pygame.display.flip()
        return True

    def handle_input(self):
        """Handles player input during gameplay"""
        keys = pygame.key.get_pressed()
        
        # Movement
        if keys[pygame.K_LEFT]:
            self.player.x -= 5
            self.player.direction = -1
            self.player.walking = True
        elif keys[pygame.K_RIGHT]:
            self.player.x += 5
            self.player.direction = 1
            self.player.walking = True
        else:
            self.player.walking = False
            
        # Abilities
        if keys[pygame.K_z]:
            self.player.use_ability('bite', self.enemy)
        if keys[pygame.K_x]:
            self.player.use_ability('acid_spray', self.enemy)
            
        # Charging power
        if keys[pygame.K_SPACE]:
            self.charging_power = min(self.charging_power + 0.5, 20)
            
        # Keep player within bounds
        self.player.x = max(10, min(self.config.SCREEN_WIDTH - 10, self.player.x))

    def update(self, dt: float):
        """Updates game state"""
        # Update entities
        self.player.update(dt, self.terrain)
        self.enemy.update(dt, self.terrain)
        self.ai_controller.update(dt, self.player, self.terrain)
        
        if self.projectile and self.projectile.active:
            self.projectile.update(self.config)
            
            # Check collisions
            if self.terrain.check_collision(int(self.projectile.x), int(self.projectile.y)):
                self.terrain.apply_destruction(int(self.projectile.x),
                                            int(self.projectile.y), 20)
                self.projectile.active = False
            
            # Check hit on enemy
            distance = math.hypot(self.projectile.x - self.enemy.x,
                                self.projectile.y - self.enemy.y)
            if distance < 15:
                self.enemy.take_damage(self.projectile.damage)
                self.projectile.active = False
        
        # Check win/lose conditions
        if not self.player.is_alive or not self.enemy.is_alive:
            self.state = GameState.GAME_OVER

    def draw(self):
        """Renders the game state"""
        self.screen.fill(Colors.WHITE)
        
        # Draw terrain
        self.screen.blit(self.terrain.surface, (0, 0))
        
        # Draw entities
        self.player.draw(self.screen)
        self.enemy.draw(self.screen)
        if self.projectile and self.projectile.active:
            self.projectile.draw(self.screen)
        
        # Draw power meter when charging
        if self.charging_power > 0:
            pygame.draw.rect(self.screen, Colors.RED,
                           (10, 10, self.charging_power * 10, 20))
        
        pygame.display.flip()

    def run(self):
        """Main game loop"""
        running = True
        last_time = time.time()
        
        while running:
            # Calculate delta time
            current_time = time.time()
            dt = current_time - last_time
            last_time = current_time
            
            # Handle game states
            if self.state == GameState.MENU:
                running = self.handle_menu()
            elif self.state == GameState.GAME_OVER:
                running = self.handle_game_over()
            else:
                # Handle events
                for event in pygame.event.get():
                    if event.type == pygame.QUIT:
                        running = False
                    elif event.type == pygame.KEYUP:
                        if event.key == pygame.K_SPACE and self.charging_power > 0:
                            # Launch projectile
                            angle = math.radians(45)
                            velocity = [
                                self.charging_power * math.cos(angle),
                                -self.charging_power * math.sin(angle)
                            ]
                            self.projectile = Projectile(self.player.x, self.player.y, velocity)
                            self.charging_power = 0
                
                self.handle_input()
                self.update(dt)
                self.draw()
            
            self.clock.tick(self.config.FPS)

        pygame.quit()
        sys.exit()

if __name__ == "__main__":
    game = Game()
    game.run()
    
