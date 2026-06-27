# ============================================================
# CHASING ROMEO — Medussa Game (Pygame)
# ============================================================
# Integrates Just Land physics, TITM matrix, and Enkimos wallet
# ============================================================

import pygame
import math
import sys
import random

# ============================================================
# JUST LAND PHYSICS ENGINE (TITM Skeletal Matrix)
# ============================================================
class SkeletalMatrixCalculator:
    def __init__(self, species_type="human"):
        self.GRAVITY = 9.80665
        self.species_type = species_type
        if species_type == "human":
            self.CONSTRAINTS = {
                "head_twist_max": math.radians(80),
                "head_tilt_max": math.radians(45),
                "limb_flex_max": math.radians(145),
                "limb_ext_min": math.radians(0)
            }
        self.velocity_y = 0.0

    def calculate_walk_cycle(self, frame_count, alpha=-0.5, gamma=0.866):
        """Computes a simple walking cycle using gravity and temporal vectors."""
        # Warp gravity slightly with TITM vectors
        warped_gravity = self.GRAVITY * (1.0 + (gamma - alpha) * 0.02)
        # Simple pendulum for leg swing
        step_angle = math.sin(frame_count * 0.1) * 0.5
        bounce = abs(math.sin(frame_count * 0.1)) * 2.0
        return step_angle, bounce

    def calculate_limb_matrices(self, extension_factor):
        span = self.CONSTRAINTS["limb_flex_max"] - self.CONSTRAINTS["limb_ext_min"]
        joint_angle = self.CONSTRAINTS["limb_ext_min"] + (extension_factor * span)
        len_upper = 1.0
        len_lower = 0.9
        joint_x = len_upper * math.sin(joint_angle)
        joint_y = -len_upper * math.cos(joint_angle)
        end_x = joint_x + len_lower * math.sin(joint_angle * 0.5)
        end_y = joint_y - len_lower * math.cos(joint_angle * 0.5)
        return {"joint_angle_rad": joint_angle, "mid_joint_node": (joint_x, joint_y), "extremity_node": (end_x, end_y)}


# ============================================================
# GAME CLASS
# ============================================================
class ChasingRomeo:
    def __init__(self):
        pygame.init()
        self.screen = pygame.display.set_mode((900, 600))
        pygame.display.set_caption("Chasing Romeo — Medussa Game")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.Font(None, 30)
        
        # Colors
        self.WHITE = (255, 255, 255)
        self.BLACK = (0, 0, 0)
        self.SKIN = (255, 200, 150)
        self.BIKINI_PINK = (255, 100, 150)
        self.BIKINI_BLUE = (100, 150, 255)
        self.ROMEO_RED = (200, 50, 50)
        self.ROMEO_GREEN = (50, 200, 50)
        self.SAND = (230, 200, 150)
        
        # Just Land Physics Engine
        self.physics = SkeletalMatrixCalculator("human")
        
        # Game state
        self.frame_count = 0
        self.score = 0
        self.chase_active = False
        self.game_over = False
        
        # Girl (Medussa)
        self.girl_x = 100
        self.girl_y = 400
        self.girl_speed = 3
        self.girl_bounce = 0
        
        # Romeo
        self.romeo_x = 700
        self.romeo_y = 400
        self.romeo_speed = 1.5
        self.romeo_fleeing = False
        self.romeo_direction = -1  # -1 = left, 1 = right
        self.romeo_fear = 0
        
        # Slap mechanics
        self.slap_animation = 0
        self.slap_range = 60
        self.slap_sound = None
        
        # TITM telemetry
        self.alpha = -0.5
        self.beta = 0.0
        self.gamma = 0.866
        
        # Enkimos wallet integration (simulated)
        self.wallet_balance = 0.0
        
        self.running = True

    def draw_girl(self):
        x, y = self.girl_x, self.girl_y + self.girl_bounce
        frame = self.frame_count
        
        # Body (bikini top)
        pygame.draw.ellipse(self.screen, self.BIKINI_PINK, (x-15, y-40, 30, 40))
        # Bikini top triangle
        pygame.draw.polygon(self.screen, self.BIKINI_PINK, [(x-12, y-35), (x+12, y-35), (x, y-25)])
        pygame.draw.polygon(self.screen, self.BIKINI_PINK, [(x-12, y-35), (x-18, y-25), (x-2, y-28)])
        pygame.draw.polygon(self.screen, self.BIKINI_PINK, [(x+12, y-35), (x+18, y-25), (x+2, y-28)])
        
        # Head (skin)
        pygame.draw.circle(self.screen, self.SKIN, (x, y-55), 18)
        # Hair (ponytail)
        for i in range(5):
            pygame.draw.ellipse(self.screen, (200, 100, 50), (x-10+i*5, y-70-i*3, 8, 15))
        
        # Eyes
        pygame.draw.circle(self.screen, self.BLACK, (x-5, y-58), 3)
        pygame.draw.circle(self.screen, self.BLACK, (x+5, y-58), 3)
        # Eyebrows (angry if chasing)
        if self.chase_active:
            pygame.draw.line(self.screen, self.BLACK, (x-10, y-63), (x-2, y-65), 2)
            pygame.draw.line(self.screen, self.BLACK, (x+10, y-63), (x+2, y-65), 2)
        else:
            pygame.draw.line(self.screen, self.BLACK, (x-10, y-65), (x-2, y-63), 2)
            pygame.draw.line(self.screen, self.BLACK, (x+10, y-65), (x+2, y-63), 2)
        
        # Smile
        if self.slap_animation > 0:
            # Slapping grin
            pygame.draw.arc(self.screen, self.BLACK, (x-8, y-52, 16, 10), 0, math.pi, 2)
        else:
            pygame.draw.arc(self.screen, self.BLACK, (x-8, y-52, 16, 10), math.pi, 2*math.pi, 2)
        
        # Bikini bottom
        pygame.draw.ellipse(self.screen, self.BIKINI_BLUE, (x-12, y-10, 24, 15))
        
        # Legs (walking cycle with Just Land physics)
        step_angle, bounce = self.physics.calculate_walk_cycle(self.frame_count)
        self.girl_bounce = bounce * 0.5
        
        # Left leg
        leg_angle = -0.3 + step_angle * 0.2
        leg_x = x - 8 + 12 * math.sin(leg_angle)
        leg_y = y + 5 + 25 * math.cos(leg_angle)
        pygame.draw.line(self.screen, self.SKIN, (x-8, y+5), (leg_x, leg_y), 6)
        # Right leg
        leg_angle2 = 0.3 - step_angle * 0.2
        leg_x2 = x + 8 + 12 * math.sin(leg_angle2)
        leg_y2 = y + 5 + 25 * math.cos(leg_angle2)
        pygame.draw.line(self.screen, self.SKIN, (x+8, y+5), (leg_x2, leg_y2), 6)
        
        # Shoes
        pygame.draw.ellipse(self.screen, (255, 50, 50), (leg_x-6, leg_y-2, 14, 6))
        pygame.draw.ellipse(self.screen, (255, 50, 50), (leg_x2-6, leg_y2-2, 14, 6))
        
        # Arms
        arm_angle = 0.2 + math.sin(self.frame_count * 0.15) * 0.3
        arm_angle2 = -0.2 + math.cos(self.frame_count * 0.15) * 0.3
        pygame.draw.line(self.screen, self.SKIN, (x-18, y-30), (x-25 + 20*math.sin(arm_angle), y-20 + 20*math.cos(arm_angle)), 5)
        pygame.draw.line(self.screen, self.SKIN, (x+18, y-30), (x+25 + 20*math.sin(arm_angle2), y-20 + 20*math.cos(arm_angle2)), 5)

    def draw_romeo(self):
        x, y = self.romeo_x, self.romeo_y
        frame = self.frame_count
        
        # Body (shirt)
        pygame.draw.ellipse(self.screen, self.ROMEO_RED, (x-15, y-30, 30, 35))
        # Collar
        pygame.draw.polygon(self.screen, (200, 50, 50), [(x-8, y-35), (x+8, y-35), (x, y-25)])
        
        # Head (skin)
        pygame.draw.circle(self.screen, self.SKIN, (x, y-48), 16)
        
        # Hair (slicked back)
        for i in range(3):
            pygame.draw.ellipse(self.screen, (50, 50, 50), (x-12+i*8, y-58, 10, 8))
        
        # Eyes (wide with fear if fleeing)
        if self.romeo_fleeing:
            # Wide eyes (scared)
            pygame.draw.circle(self.screen, self.WHITE, (x-6, y-52), 5)
            pygame.draw.circle(self.screen, self.WHITE, (x+6, y-52), 5)
            pygame.draw.circle(self.screen, self.BLACK, (x-6, y-52), 3)
            pygame.draw.circle(self.screen, self.BLACK, (x+6, y-52), 3)
        else:
            # Normal eyes (confident)
            pygame.draw.circle(self.screen, self.WHITE, (x-6, y-52), 4)
            pygame.draw.circle(self.screen, self.WHITE, (x+6, y-52), 4)
            pygame.draw.circle(self.screen, self.BLACK, (x-5, y-51), 2)
            pygame.draw.circle(self.screen, self.BLACK, (x+5, y-51), 2)
            # Eyebrows (raised)
            pygame.draw.line(self.screen, self.BLACK, (x-10, y-58), (x-2, y-56), 2)
            pygame.draw.line(self.screen, self.BLACK, (x+10, y-58), (x+2, y-56), 2)
        
        # Mouth
        if self.romeo_fleeing:
            # Open mouth (screaming)
            pygame.draw.ellipse(self.screen, self.BLACK, (x-4, y-40, 8, 8))
        else:
            # Smirk
            pygame.draw.arc(self.screen, self.BLACK, (x-8, y-44, 16, 8), math.pi, 2*math.pi, 2)
        
        # Legs
        step_angle, bounce = self.physics.calculate_walk_cycle(self.frame_count * 1.2)
        leg_angle = -0.3 + step_angle * 0.3
        leg_x = x - 6 + 10 * math.sin(leg_angle)
        leg_y = y + 5 + 20 * math.cos(leg_angle)
        pygame.draw.line(self.screen, (50, 100, 200), (x-6, y+5), (leg_x, leg_y), 5)
        leg_angle2 = 0.3 - step_angle * 0.3
        leg_x2 = x + 6 + 10 * math.sin(leg_angle2)
        leg_y2 = y + 5 + 20 * math.cos(leg_angle2)
        pygame.draw.line(self.screen, (50, 100, 200), (x+6, y+5), (leg_x2, leg_y2), 5)
        
        # Shoes (fancy)
        pygame.draw.ellipse(self.screen, (100, 100, 100), (leg_x-8, leg_y-2, 16, 6))
        pygame.draw.ellipse(self.screen, (100, 100, 100), (leg_x2-8, leg_y2-2, 16, 6))
        
        # Arms (flailing if fleeing)
        if self.romeo_fleeing:
            # Panic arms
            arm_angle1 = math.sin(frame * 0.2) * 0.8
            arm_angle2 = math.cos(frame * 0.2) * 0.8
            pygame.draw.line(self.screen, self.SKIN, (x-18, y-25), (x-25 + 15*math.sin(arm_angle1), y-15 + 15*math.cos(arm_angle1)), 4)
            pygame.draw.line(self.screen, self.SKIN, (x+18, y-25), (x+25 + 15*math.sin(arm_angle2), y-15 + 15*math.cos(arm_angle2)), 4)
        else:
            # Confident arms
            pygame.draw.line(self.screen, self.SKIN, (x-18, y-25), (x-30, y-15), 4)
            pygame.draw.line(self.screen, self.SKIN, (x+18, y-25), (x+30, y-15), 4)

    def draw_scene(self):
        # Sky gradient
        for i in range(600):
            color = (100 + i//3, 150 + i//4, 200 + i//10)
            pygame.draw.line(self.screen, color, (0, i), (900, i))
        
        # Sand (beach/street)
        pygame.draw.rect(self.screen, self.SAND, (0, 450, 900, 150))
        # Texture dots
        for _ in range(50):
            pygame.draw.circle(self.screen, (200, 180, 140), (random.randint(0, 900), random.randint(460, 580)), 2)
        
        # Sun
        pygame.draw.circle(self.screen, (255, 255, 100), (800, 80), 50)
        
        # Clouds
        for cloud in self.clouds:
            pygame.draw.ellipse(self.screen, (255, 255, 255, 100), cloud)
        
        # Palm trees (background)
        pygame.draw.rect(self.screen, (100, 80, 50), (30, 370, 15, 80))
        for leaf in range(5):
            angle = leaf * math.pi/2.5
            pygame.draw.line(self.screen, (50, 150, 50), (37, 370), (37 + 50*math.cos(angle), 370 - 50*math.sin(angle)), 8)
        pygame.draw.rect(self.screen, (100, 80, 50), (870, 370, 15, 80))
        for leaf in range(5):
            angle = leaf * math.pi/2.5 + 0.5
            pygame.draw.line(self.screen, (50, 150, 50), (877, 370), (877 + 50*math.cos(angle), 370 - 50*math.sin(angle)), 8)

    def handle_events(self):
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False
            
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_SPACE and not self.chase_active:
                    # Slap!
                    self.slap_animation = 15
                    dist = math.hypot(self.girl_x - self.romeo_x, self.girl_y - self.romeo_y)
                    if dist < self.slap_range:
                        self.romeo_fleeing = True
                        self.chase_active = True
                        self.romeo_speed = 3.5
                        self.romeo_fear = 100
                        self.score += 1
                        # Direction: Romeo runs away from girl
                        if self.romeo_x > self.girl_x:
                            self.romeo_direction = 1
                        else:
                            self.romeo_direction = -1
                        # Simulate Enkimos wallet deposit
                        self.wallet_balance += 0.00000001
                elif event.key == pygame.K_r and self.game_over:
                    self.reset_game()

    def update(self):
        self.frame_count += 1
        if self.slap_animation > 0:
            self.slap_animation -= 1
        
        # Girl movement (WASD)
        keys = pygame.key.get_pressed()
        dx, dy = 0, 0
        if keys[pygame.K_a]:
            dx = -self.girl_speed
        if keys[pygame.K_d]:
            dx = self.girl_speed
        if keys[pygame.K_w]:
            dy = -self.girl_speed
        if keys[pygame.K_s]:
            dy = self.girl_speed
        
        # Apply movement
        self.girl_x += dx
        self.girl_y += dy
        
        # Keep girl in bounds
        self.girl_x = max(20, min(880, self.girl_x))
        self.girl_y = max(300, min(550, self.girl_y))
        
        # Romeo logic
        if self.romeo_fleeing:
            # Chase logic
            self.romeo_x += self.romeo_speed * self.romeo_direction
            self.romeo_fear -= 0.5
            
            # Romeo bounces off walls
            if self.romeo_x < 20 or self.romeo_x > 880:
                self.romeo_direction *= -1
            
            # Check if caught
            dist = math.hypot(self.girl_x - self.romeo_x, self.girl_y - self.romeo_y)
            if dist < 35:
                self.game_over = True
                self.chase_active = False
                self.romeo_fleeing = False
                self.romeo_speed = 1.5
                print("🏆 CAUGHT! Score:", self.score)
            
            # If fear runs out, Romeo escapes
            if self.romeo_fear <= 0:
                self.romeo_fleeing = False
                self.chase_active = False
                self.romeo_speed = 1.5
                self.romeo_fear = 0
                print("😤 Romeo escaped!")
        
        # TITM matrix state update (for physics)
        self.alpha = -0.5 + math.sin(self.frame_count * 0.01) * 0.2
        self.beta = 0.2 * math.cos(self.frame_count * 0.005)
        self.gamma = 0.866 + math.cos(self.frame_count * 0.01) * 0.1

    def draw_hud(self):
        # Score
        score_text = self.font.render(f"Score: {self.score}", True, self.WHITE)
        self.screen.blit(score_text, (20, 20))
        
        # Wallet balance (Enkimos)
        balance_text = self.font.render(f"Wallet: {self.wallet_balance:.8f} XMR", True, (100, 200, 255))
        self.screen.blit(balance_text, (20, 50))
        
        # TITM telemetry
        alpha_text = self.font.render(f"α: {self.alpha:.2f}", True, (0, 255, 200))
        beta_text = self.font.render(f"β: {self.beta:.2f}", True, (0, 255, 200))
        gamma_text = self.font.render(f"γ: {self.gamma:.2f}", True, (0, 255, 200))
        self.screen.blit(alpha_text, (720, 20))
        self.screen.blit(beta_text, (720, 45))
        self.screen.blit(gamma_text, (720, 70))
        
        # Status
        if self.chase_active:
            status = "CHASING! 🏃‍♀️"
            status_color = (255, 100, 100)
        elif self.game_over:
            status = "CAUGHT! 🏆 Press R to restart"
            status_color = (255, 200, 50)
        else:
            status = "Press SPACE to slap Romeo"
            status_color = (200, 255, 200)
        status_text = self.font.render(status, True, status_color)
        self.screen.blit(status_text, (300, 20))
        
        # Instructions
        if not self.game_over and not self.chase_active:
            instr = self.font.render("WASD: Move", True, (150, 150, 150))
            self.screen.blit(instr, (20, 550))
        if self.game_over:
            restart_text = self.font.render("Press R to restart", True, (255, 200, 100))
            self.screen.blit(restart_text, (350, 550))
        
        # Slap animation indicator
        if self.slap_animation > 0:
            slap_text = self.font.render("💥 SLAP!", True, (255, 50, 50))
            self.screen.blit(slap_text, (self.girl_x - 30, self.girl_y - 90))

    def reset_game(self):
        self.game_over = False
        self.chase_active = False
        self.romeo_fleeing = False
        self.romeo_x = 700
        self.romeo_y = 400
        self.romeo_speed = 1.5
        self.romeo_fear = 0
        self.girl_x = 100
        self.girl_y = 400
        self.slap_animation = 0
        self.wallet_balance = 0.0

    def draw_scene(self):
        # Sky gradient
        for i in range(600):
            color = (100 + i//3, 150 + i//4, 200 + i//10)
            pygame.draw.line(self.screen, color, (0, i), (900, i))
        
        # Sand
        pygame.draw.rect(self.screen, (230, 200, 150), (0, 450, 900, 150))
        
        # Sun
        pygame.draw.circle(self.screen, (255, 255, 100), (800, 80), 50)
        
        # Clouds
        if not hasattr(self, 'clouds'):
            self.clouds = []
            for _ in range(5):
                x = random.randint(100, 800)
                y = random.randint(50, 200)
                w = random.randint(80, 200)
                self.clouds.append((x, y, w, 30))
        for cloud in self.clouds:
            pygame.draw.ellipse(self.screen, (255, 255, 255, 100), cloud)

    def run(self):
        while self.running:
            self.handle_events()
            self.update()
            
            # Draw everything
            self.draw_scene()
            self.draw_girl()
            self.draw_romeo()
            self.draw_hud()
            
            pygame.display.flip()
            self.clock.tick(60)
        
        pygame.quit()
        sys.exit()


if __name__ == "__main__":
    game = ChasingRomeo()
    game.run()
